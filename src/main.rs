use std::{path::Path, vec, mem};
use capsule::capsule_main;
use config::{ConfItem, ConfItemTrait};
use error::GemcapsResult;
use futures::future;

use clap::Parser;
use log::Logger;
use tokio::{fs, io::AsyncReadExt};
use lazy_static::lazy_static;

use crate::{error::GemcapsError, config::{ConfDeserializer, CapsuleConf}, pem::Cert};

mod capsule;
mod pathutil;
mod config;
mod error;
mod tls;
mod glob;
mod pem;
mod gemini;
mod log;

/// 
#[derive(Parser, Debug)]
#[clap(author, version, about, long_about = None)]
pub struct Args {
    /// Configuration folder, this is where you can place all capsule configs
    #[clap(short, long, default_value = ".")]
    config: String,
    /// Logs folder, default location for logging output
    #[clap(short, long)]
    logs: Option<String>,
}

pub fn args() -> &'static Args {
    lazy_static! {
        static ref ARGS: Args = Args::parse();
    }
    &ARGS
}

async fn runner() -> GemcapsResult<()> {
    let args = args();

    let config = Path::new(&args.config);
    let logs = args.logs.as_ref().map(|logs| Path::new(logs)).or(Some(config)).unwrap();
    let capsules = config.join("capsules");

    let mut read_dir = match fs::read_dir(&capsules).await {
        Ok(val) => val,
        Err(e) => {
            return Err(GemcapsError::new(
                format!("Could not read config directory {:?}: {}", capsules, e)
            ));
        }
    };
    let mut futures = vec![];
    while let Some(entry) = read_dir.next_entry().await.unwrap() {
        let mut file = fs::File::open(entry.path()).await?;
        let mut buf = vec![];

        let name = entry.file_name().to_string_lossy().to_string();

        file.read_to_end(&mut buf).await?;

        let conf: ConfDeserializer = toml::from_str(&String::from_utf8(buf)?).map_err(
            |err| GemcapsError::new(
                format!("Invalid configuration in {}: {}", name, err)
            )
        )?;
        let mut conf: CapsuleConf = conf.try_into().map_err(
            |err| GemcapsError::new(
                format!("Invalid configuration in {}: {}", name, err)
            )
        )?;
        conf.name = match name.rfind(".") {
            Some(pos) => {
                name[0..pos].to_owned()
            },
            None => name
        };

        if let Some(certificate) = conf.certificate.as_mut() {
            let cert = mem::take(certificate);
            if let Cert::Closed(mut cert) = cert {
                cert.relative(&config);
                mem::drop(mem::replace(
                    certificate, 
                    Cert::Opened(cert.open().map_err(|err| GemcapsError::new(
                        format!("Unable to read certificate for {}: {}", conf.name, err)
                    ))?)
                ));
            }
        }

        for item in &mut conf.items {
            if let ConfItem::File(app) = item {
                if app.file_root.is_relative() {
                    app.file_root = config.join(&app.file_root);
                }
            }
            if let ConfItem::CGI(app) = item {
                if app.cgi_root.is_relative() {
                    app.cgi_root = config.join(&app.cgi_root);
                }
            }
            if let Some(error_log) = item.error_log_mut() {
                if error_log.is_relative() {
                    drop(mem::replace(error_log, logs.join(&error_log)));
                }
            }
            if let Some(access_log) = item.access_log_mut() {
                if access_log.is_relative() {
                    drop(mem::replace(access_log, logs.join(&access_log)));
                }
            }
        }

        if let Some(error_log) = conf.error_log.as_mut() {
            if error_log.is_relative() {
                drop(mem::replace(error_log, logs.join(&error_log)));
            }
        }

        let mut logger = Logger::new(&conf.name);
        if let Some(error_log) = &conf.error_log {
            logger.set_error(error_log).await?;
        }
        futures.push(async move {
            match capsule_main(conf, &mut logger).await {
                Ok(_) => {},
                Err(err) => {
                    logger.error(err.to_string()).await;
                }
            }
        });
    }

    future::join_all(futures).await;

    Ok(())
}




#[tokio::main]
async fn main() {
    match runner().await {
        Ok(_) => {},
        Err(err) => {
            let mut logger = Logger::new("main");
            logger.error(err.to_string()).await;
        }
    }
}
