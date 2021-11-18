use std::sync::Arc;

use crate::server::SniCert;

mod settings;
mod server;
mod pem;
mod gemini;
mod capsule;
mod pathutil;
mod cache;

#[tokio::main]
async fn main() {

    let config_dir = match std::env::args().nth(1) {
        Some(arg) => arg,
        None => {
            eprintln!("Usage: {} CONFIG", std::env::args().nth(0).unwrap_or("gemcaps".to_string()));
            return;
        }
    };

    let sett = match settings::load_settings(&config_dir).await {
        Ok(val) => val,
        Err(e) => {
            eprintln!("Could not load settings: {}", e.to_string());
            return;
        }
    };

    let mut sni_certs: Vec<SniCert> = Vec::new();

    for (server, cert) in &sett.certificates {
        let cert = match server::SniCert::load(
            server, 
            &pathutil::abspath(&config_dir, &cert.cert),
            &pathutil::abspath(&config_dir,&cert.key)
        ) {
            Ok(cert) => cert,
            Err(err) => {
                eprintln!("Could not read certificate '{}': {}", server, err.to_string());
                return;
            }
        };
        sni_certs.push(cert);
    }
    
    let loaders: [Arc<dyn capsule::Loader>; 1] = [
        Arc::new(capsule::files::FileConfigLoader)
    ];
    let capsules = match capsule::load_capsules(
        &pathutil::abspath(&config_dir, &sett.capsules),
        &loaders
    ).await {
        Ok(capsules) => capsules,
        Err(err) => {
            eprintln!("Error while loading capsules: {}", err.to_string());
            return;
        }
    };

    if let Err(err) = server::serve(&sett.listen, &sett, sni_certs, capsules).await {
        eprintln!("Error: {}", err.to_string());
    }
}
