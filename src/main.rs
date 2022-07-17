use std::{path::Path, vec, mem};
use config::ConfItem;
use futures::future;

use clap::Parser;
use runner::capsule_main;
use tokio::{fs, io::AsyncReadExt};

use crate::{error::GemcapsError, config::{Configuration, CapsuleConf}, pem::Cert};

mod settings;
mod server;
mod capsule;
mod pathutil;
mod cache;

mod config;
mod error;
mod runner;
mod tls;
mod glob;
mod pem;
mod gemini;

/// 
#[derive(Parser, Debug)]
#[clap(author, version, about, long_about = None)]
struct Args {
    /// Configuration folder, this is where you can place all capsule configs
    #[clap(short, long, default_value = ".")]
    config: String,
    /// Logs folder, default location for logging output
    #[clap(short, long, default_value = "./logs/")]
    logs: String,
}

async fn runner() -> Result<(), GemcapsError> {
    let args = Args::parse();

    let config = Path::new(&args.config);
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

        let conf: Configuration = toml::from_str(&String::from_utf8(buf)?).map_err(
            |err| GemcapsError::new(
                format!("Invalid configuration in {}: {}", name, err)
            )
        )?;
        let mut conf: CapsuleConf = conf.try_into().map_err(
            |err| GemcapsError::new(
                format!("Invalid configuration in {}: {}", name, err)
            )
        )?;
        conf.name = name;

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
        }

        futures.push(async move {
            let name = conf.name.to_owned();
            match capsule_main(conf).await {
                Ok(_) => {},
                Err(err) => {
                    eprintln!("[{}]: {}", name, err);
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
            eprintln!("{}", err);
        }
    }
    // let sett = match settings::load_settings(&config_dir).await {
    //     Ok(val) => val,
    //     Err(e) => {
    //         eprintln!("Could not load settings: {}", e.to_string());
    //         return;
    //     }
    // };

    // let mut sni_certs: Vec<SniCert> = Vec::new();

    // for (server, cert) in &sett.certificates {
    //     let cert = match server::SniCert::load(
    //         server, 
    //         pathutil::abspath(&config_dir, &cert.cert),
    //         pathutil::abspath(&config_dir,&cert.key)
    //     ) {
    //         Ok(cert) => cert,
    //         Err(err) => {
    //             eprintln!("Could not read certificate '{}': {}", server, err.to_string());
    //             return;
    //         }
    //     };
    //     sni_certs.push(cert);
    // }
    
    // let loaders: [Arc<dyn capsule::Loader>; 1] = [
    //     Arc::new(capsule::files::FileConfigLoader)
    // ];
    // let capsules = match capsule::load_capsules(
    //     pathutil::abspath(&config_dir, &sett.capsules),
    //     &loaders
    // ).await {
    //     Ok(capsules) => capsules,
    //     Err(err) => {
    //         eprintln!("Error while loading capsules: {}", err.to_string());
    //         return;
    //     }
    // };

    // if let Err(err) = server::serve(&sett.listen, &sett, sni_certs, capsules).await {
    //     eprintln!("Error: {}", err.to_string());
    // }
}
