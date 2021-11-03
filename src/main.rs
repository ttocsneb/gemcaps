use crate::server::SniCert;

mod settings;
// mod files;
mod server;
mod pem;

#[tokio::main]
async fn main() {

    let sett = match settings::load_settings().await {
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
            &cert.cert,
            &cert.key
        ).unwrap());
    }

    server::serve(&sett.listen, &sni_certs).await.unwrap();
}
