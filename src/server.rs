use tokio::net::{TcpListener};
use tokio_rustls::rustls::{self, Certificate, PrivateKey, sign, server};
use tokio_rustls::TlsAcceptor;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::sync::Arc;
use std::str;

use crate::{capsule, pem};
use crate::gemini;

pub struct SniCert {
    name: String,
    cert: Vec<Certificate>,
    key: PrivateKey,
}

impl SniCert {
    pub fn load(server_name: &str, cert_file: &str, key_file: &str) -> Result<Self, Box<dyn std::error::Error>> {
        let cert = pem::read_cert(&cert_file)?;
        let key = pem::read_key(&key_file)?;
        Ok(SniCert {
            name: String::from(server_name),
            cert,
            key,
        })
    }
}

pub async fn serve(listen: &str, certs: Vec<SniCert>, capsules: Vec<Arc<dyn capsule::Capsule>>) -> Result<(), Box<dyn std::error::Error>> {
    let mut sni = server::ResolvesServerCertUsingSni::new();

    // let acapsules = Arc::new(capsules);

    for cert in &certs {
        println!("Loading certificate for {}", cert.name);
        let key = sign::any_supported_type(&cert.key)
            .map_err(|_| rustls::Error::General("invalid private key".into()))?;
        sni.add(&cert.name, sign::CertifiedKey::new(cert.cert.clone(), key))?;
    }

    println!("Creating Config");
    let config = rustls::ServerConfig::builder()
        .with_safe_defaults()
        .with_no_client_auth() // TODO Make Client Auth Resolver
        .with_cert_resolver(Arc::new(sni));

    let acceptor = TlsAcceptor::from(Arc::new(config));
    let listener = TcpListener::bind(&listen).await?;
    println!("Listening on {}", listen);

    loop {
        let (stream, peer_addr) = listener.accept().await?;
        let acceptor = acceptor.clone();
        let caps = capsules.clone();
        println!("Receiving request from {:?}", peer_addr);


        let fut = async move {
            let mut stream = acceptor.accept(stream).await?;

            // Read the request
            let mut buf = [0; 1024];
            let n = stream.read(&mut buf).await?;

            // Convert the request into utf8 (TODO: use standard url encoding)
            let header = str::from_utf8(&buf[0 .. n])?;

            // Parse the request
            let request = gemini::Request::parse(header)?;

            // Process the request
            for capsule in caps {
                if !capsule.get_capsule().test(&request.domain, &request.path) {
                    continue;
                }
                if !capsule.test(&request.domain, &request.path) {
                    continue;
                }
                let response = match capsule.serve(&request).await {
                    Ok(response) => response,
                    Err(err) => {
                        eprint!("Error while generating response: {:?}", err);
                        format!(
                            "42 An error occurred while generating your response\r\n"
                        )
                    }
                };
                stream.write(response.as_bytes()).await?;
                stream.shutdown().await?;
                return Ok(()) as Result<(), Box<dyn std::error::Error>>;
            }

            // Send the response for no willing capsules
            let response = "51 There are no capsules willing to take your request\r\n";
            stream.write(&response.as_bytes()).await?;
            stream.shutdown().await?;

            Ok(()) as Result<(), Box<dyn std::error::Error>>
        };

        tokio::spawn(async move {
            if let Err(err) = fut.await {
                eprint!("Error while handling request: {:?}", err);
            }
        });
    }
}