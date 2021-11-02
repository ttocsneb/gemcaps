use crate::server::SniCert;

mod settings;
// mod files;
mod server;
mod pem;

#[tokio::main]
async fn main() {

    let sett = settings::load_settings().await.unwrap();

    let mut sni_certs: Vec<SniCert> = Vec::new();

    for (server, cert) in &sett.certificates {
        sni_certs.push(server::SniCert::new(
            server.clone(), 
            pem::read_cert(cert.cert.as_str()).unwrap(), 
            pem::read_key(cert.key.as_str()).unwrap()).unwrap()
        );
    }

    server::serve(&sett.listen, &sni_certs).await.unwrap();
}
