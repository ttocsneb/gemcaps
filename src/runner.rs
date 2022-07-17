use std::mem;
use std::net::SocketAddr;
use std::sync::Arc;

use tokio::io::{AsyncWriteExt, AsyncReadExt};
use tokio::join;
use tokio::net::tcp::{ReadHalf, WriteHalf};
use tokio::net::{TcpListener, TcpStream};

use crate::config::ConfItem;
use crate::gemini::Request;
use crate::pem::Cert;
use crate::tls::connection::Connection;
use crate::{config::CapsuleConf, error::GemcapsError};
use crate::tls::acceptor::Acceptor;


async fn handle_request(conf: &CapsuleConf, mut stream: TcpStream, addr: SocketAddr) -> Result<(), GemcapsError> {

    let log_msg = |msg: &str| {
        println!("[{}] {}: {}", conf.name, addr.ip(), msg);
    };
    let err_msg = |msg: &str| {
        eprintln!("[{}] {}: {}", conf.name, addr.ip(), msg);
    };

    let mut acceptor = Acceptor::new()?;
    let accepted = acceptor.accept(&mut stream).await?;

    let sni = accepted.client_hello().server_name().or(Some("")).unwrap().to_owned();

    let mut applications = vec![];
    for conf in &conf.items {
        for name in conf.domain_names() {
            if name.matches(&sni) {
                applications.push(conf);
                break;
            }
        }
    }
    
    if let Some(ConfItem::Redirect(application)) = applications.first() {
        let mut redirect = TcpStream::connect(&application.redirect).await?;
        redirect.write_all(&acceptor.buffer.buf).await?;

        let (redirect_read, redirect_write) = redirect.split();
        let (stream_read, stream_write) = stream.split();

        async fn forward_stream<'a>(mut rd: ReadHalf<'a>, mut wd: WriteHalf<'a>) -> Result<(), GemcapsError> {
            let mut buf = vec![];
            loop {
                if rd.read_buf(&mut buf).await? == 0 {
                    return Ok(());
                };
                wd.write_all(&buf).await?;
                buf.clear();
            }
        }

        join!(async move {
            match forward_stream(redirect_read, stream_write).await {
                Ok(_) => {},
                Err(err) => {
                    err_msg(&format!("{}", err));
                }
            }
        }, async move {
            match forward_stream(stream_read, redirect_write).await {
                Ok(_) => {},
                Err(err) => {
                    err_msg(&format!("{}", err));
                }
            }
        });
        log_msg(&format!("Redirected to {}", &application.redirect));
        return Ok(());
    }

    let server_conf = conf.server_conf.as_ref().ok_or_else(
        || GemcapsError::new("A certificate is required to communicate")
    )?.to_owned();
    let connection = accepted.into_connection(server_conf)?;
    let mut connection = Connection::new(connection, stream);
    connection.complete_io().await?;

    if applications.len() == 0 {
        connection.send("53 Requested domain not served here\r\n").await?;
        return Err(GemcapsError::new("No application is willing to process the request"));
    }

    let request = connection.receive(1024).await?;
    let request = Request::new(request).ok();
    let request = match request {
        Some(val) => val,
        None => {
            connection.send("59 Invalid request format\r\n").await?;
            return Err(GemcapsError::new("Invalid request format"));
        }
    };

    for application in applications {
        match application {
            ConfItem::Redirect(_) => {
                log_msg("Warning, redirects must be the first option available");
            },
            ConfItem::Proxy(_application) => {
                log_msg("Trying Proxy");
            },
            ConfItem::CGI(_application) => {
                log_msg("Trying CGI");
            },
            ConfItem::File(_application) => {
                log_msg("Trying File");
            },
        }
    }

    connection.send("51 Resource not found\r\n").await?;
    Err(GemcapsError::new("No resource avalable"))
}


pub async fn capsule_main(mut conf: CapsuleConf) -> Result<(), GemcapsError> {
    let listener = TcpListener::bind(&conf.listen).await?;
    
    let cert = mem::replace(&mut conf.certificate, None);
    conf.server_conf = if let Some(cert) = cert {
        if let Cert::Opened(cert) = cert {
            let config = rustls::ServerConfig::builder()
                .with_safe_defaults()
                .with_no_client_auth()
                .with_single_cert(cert.cert, cert.key)?;
            Some(Arc::new(config))
        } else {
            return Err(GemcapsError::new("Certificate is not opened"));
        }
    } else {
        None
    };

    // Assert that the server_conf has been loaded if it is necessary for the config
    if let None = conf.server_conf {
        for item in &conf.items {
            if let ConfItem::Redirect(_) = item {}
            else {
                return Err(GemcapsError::new("Certificate is required for non-redirect applications"));
            }
        }
    }

    println!("[{}]: Listening on {}", conf.name, conf.listen);

    let conf = Arc::new(conf);
    loop {
        let (stream, addr) = listener.accept().await?;
        
        let conf = conf.to_owned();
        tokio::spawn(async move {
            let ip = addr.ip();
            match handle_request(&conf, stream, addr).await {
                Ok(()) => {},
                Err(err) => {
                    eprintln!("[{}] {}: {}", conf.name, ip, err);
                },
            }
        });
    }
}
