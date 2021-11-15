use std::sync::Arc;

use crate::server::SniCert;

mod settings;
mod server;
mod pem;
mod gemini;
mod capsule;
mod pathutil;

#[tokio::main]
async fn main() {

    let config_dir = std::env::args().nth(1).expect("A config directory was expected");

    let sett = match settings::load_settings(&config_dir).await {
        Ok(val) => val,
        Err(e) => {
            println!("Could not load settings: {}", e.to_string());
            return;
        }
    };

    let mut sni_certs: Vec<SniCert> = Vec::new();

    for (server, cert) in &sett.certificates {
        sni_certs.push(server::SniCert::load(
            server, 
            &pathutil::abspath(&config_dir, &cert.cert),
            &pathutil::abspath(&config_dir,&cert.key)
        ).unwrap());
    }
    
    let loaders: [Arc<dyn capsule::Loader>; 1] = [
        Arc::new(capsule::files::FileConfigLoader)
    ];
    let capsules = capsule::load_capsules(
        &pathutil::abspath(&config_dir, &sett.capsules),
        &loaders
    ).await.unwrap();

    server::serve(&sett.listen, sni_certs, capsules).await.unwrap();
}
