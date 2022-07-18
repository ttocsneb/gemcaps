use std::mem;
use std::net::SocketAddr;
use std::sync::Arc;

use tokio::net::{TcpListener, TcpStream};

use crate::capsule::{redirect, CapsuleResponse, file};
use crate::config::ConfItem;
use crate::gemini::Request;
use crate::log::Logger;
use crate::pem::Cert;
use crate::tls::connection::Connection;
use crate::{config::CapsuleConf, error::GemcapsError};
use crate::tls::acceptor::Acceptor;


async fn handle_request(conf: &CapsuleConf, mut stream: TcpStream, _addr: SocketAddr, logger: &mut Logger) -> Result<(), GemcapsError> {

    let mut acceptor = Acceptor::new()?;
    let accepted = acceptor.accept(&mut stream).await?;

    let sni = accepted.client_hello().server_name().or(Some("")).unwrap().to_owned();

    if redirect::redirect(&conf, &sni, &acceptor.buffer.buf, &mut stream, &logger).await? {
        return Ok(());
    }
    drop(acceptor);

    let server_conf = conf.server_conf.as_ref().ok_or_else(
        || GemcapsError::new("A certificate is required to communicate")
    )?.to_owned();
    let connection = accepted.into_connection(server_conf)?;
    let mut connection = Connection::new(connection, stream);
    connection.complete_io().await?;

    let request = connection.receive(1024).await?;
    let request = Request::new(request).ok();
    let request = match request {
        Some(val) => val,
        None => {
            connection.send(CapsuleResponse::bad_request("Invalid request format").to_string()).await?;
            return Err(GemcapsError::new("Invalid request format"));
        }
    };

    let domain = request.domain();


    let mut applications = vec![];
    for conf in &conf.items {
        for name in conf.domain_names() {
            if name.matches(&domain) {
                applications.push(conf);
                break;
            }
        }
    }

    if applications.len() == 0 {
        connection.send(CapsuleResponse::not_found("Requested application not served here").to_string()).await?;
        return Err(GemcapsError::new("No application is willing to process the request"));
    }

    for application in applications {
        let mut logger = logger.as_replace_logs(application.access_log().map(|v| v.clone()), application.error_log().map(|v| v.clone())).await?;
        let response = match application {
            ConfItem::Redirect(_) => {
                logger.error("Redirects must be the first option available").await;
                Ok(None)
            },
            ConfItem::Proxy(_application) => {
                logger.info("Proxy not yet implemented");
                Ok(None)
            },
            ConfItem::CGI(_application) => {
                logger.info("CGI not yet implemented");
                Ok(None)
            },
            ConfItem::File(application) => {
                file::process_file_request(&request, application, &mut logger).await
            },
        };
        match response {
            Ok(Some(response)) => {
                connection.send(response.to_string()).await?;
                logger.access(response.info()).await;
                return Ok(());
            },
            Err(err) => {
                connection.send(CapsuleResponse::failure_perm("Internal server error").to_string()).await?;
                logger.error(err.to_string()).await;
            },
            _ => {},
        }
    }

    connection.send(CapsuleResponse::not_found("Resource not found").to_string()).await?;
    Err(GemcapsError::new("No resource avalable"))
}


pub async fn capsule_main(mut conf: CapsuleConf, logger: &mut Logger) -> Result<(), GemcapsError> {
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

    logger.info(format!("Listening on {}", conf.listen));

    let conf = Arc::new(conf);
    loop {
        let (stream, addr) = listener.accept().await?;
        
        let conf = conf.to_owned();
        let mut logger = logger.as_option(format!("{}:{}", addr.ip(), addr.port()));
        tokio::spawn(async move {
            match handle_request(&conf, stream, addr, &mut logger).await {
                Ok(()) => {},
                Err(err) => {
                    logger.error(err.to_string()).await;
                },
            }
        });
    }
}
