
use tokio::{net::{TcpStream, tcp::{ReadHalf, WriteHalf}}, io::{AsyncWriteExt, AsyncReadExt}, join};

use crate::{log::Logger, error::GemcapsError, config::{CapsuleConf, ConfItem, redirect::RedirectConf}};


pub async fn redirect(conf: &CapsuleConf, sni: &str, client_hello: &[u8], stream: &mut TcpStream, logger: &Logger) -> Result<bool, GemcapsError> {
    fn get_application<'t>(conf: &'t CapsuleConf, sni: &str) -> Option<&'t RedirectConf> {
        for conf in &conf.items {
            if let ConfItem::Redirect(conf) = conf {
                for name in &conf.domain_names {
                    if name.matches(&sni) {
                        return Some(&conf);
                    }
                }
            }
        }
        None
    }
    let application = match get_application(&conf, &sni) {
        Some(app) => app,
        None => {
            return Ok(false);
        }
    };

    let mut logger = logger.as_logs(application.access_log.to_owned(), application.error_log.to_owned()).await?;
    let mut redirect = TcpStream::connect(&application.redirect).await?;
    redirect.write_all(client_hello).await?;

    logger.access(format!("Redirect to {}", &application.redirect)).await;

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
    let mut logger_a = logger.clone();
    let mut logger_b = logger.clone();
    join!(async move {
        match forward_stream(redirect_read, stream_write).await {
            Ok(_) => {},
            Err(err) => {
                logger_a.error(err.to_string()).await;
            }
        }
    }, async move {
        match forward_stream(stream_read, redirect_write).await {
            Ok(_) => {},
            Err(err) => {
                logger_b.error(err.to_string()).await;
            }
        }
    });
    return Ok(true);
}