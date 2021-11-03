use rustls::{Certificate, PrivateKey, server::ResolvesServerCertUsingSni, sign, Error};
use tokio::net::{TcpListener, TcpStream};
use std::sync::Arc;

use crate::pem;

// Using https://github.com/rustls/rustls/blob/main/rustls-mio/examples/tlsserver.rs as a tutorial

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


struct TLSServer {
    server: TcpListener,
    tls_config: Arc<rustls::ServerConfig>,
}

impl TLSServer {
    fn new(server: TcpListener, cfg: Arc<rustls::ServerConfig>) -> Self {
        TLSServer {
            server,
            tls_config: cfg
        }
    }

    async fn accept(&mut self) -> TLSClient {
        loop {
            match self.server.accept().await {
                Ok((socket, addr)) => {
                    println!("Accepting new connection from {:?}", addr);
                    let tls_conn = rustls::ServerConnection::new(Arc::clone(&self.tls_config)).unwrap();
                    let connection = TLSClient::new(socket, tls_conn);
                    return connection;
                }
                Err(e) => {
                    println!("Couldn't accept client: {:?}", e);
                }
            };
        }
    }
}

struct TLSClient {
    socket: TcpStream,
    conn: rustls::ServerConnection,
}

impl TLSClient {
    fn new(socket: TcpStream, conn: rustls::ServerConnection) -> Self {
        TLSClient {
            socket,
            conn
        }
    }

    async fn read(&self) {

    }

    async fn write(&self) {

    }
}

pub async fn serve(listen: &str, certs: &Vec<SniCert>) -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind(listen).await?;
    let mut sni = ResolvesServerCertUsingSni::new();

    for cert in certs {
        println!("Loading certificate for {}", cert.name);
        let key = sign::any_supported_type(&cert.key)
            .map_err(|_| Error::General("invalid private key".into()))?;
        sni.add(&cert.name, sign::CertifiedKey::new(cert.cert.clone(), key))?;
    }

    println!("Creating Config");
    let config = rustls::ServerConfig::builder()
        .with_safe_defaults()
        .with_no_client_auth() // TODO Make Client Auth Resolver
        .with_cert_resolver(Arc::new(sni));
    let mut server = TLSServer::new(listener, Arc::new(config));
    println!("Listening on {}", listen);
    loop {
        let connection = server.accept().await;
    }
}