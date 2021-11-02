use rustls::{Certificate, PrivateKey, server::ResolvesServerCertUsingSni, sign::{CertifiedKey, RsaSigningKey}};
use tokio::{net::{TcpListener, TcpStream}};
use std::{io, sync::Arc};

// Using https://github.com/rustls/rustls/blob/main/rustls-mio/examples/tlsserver.rs as a tutorial

pub struct SniCert {
    cert_name: String,
    cert: CertifiedKey
}

impl SniCert {
    pub fn new(server_name: String, cert: Vec<u8>, key: Vec<u8>) -> io::Result<Self> {
        let mut certificate: Vec<Certificate> = Vec::new();
        certificate.push(Certificate(cert));
        let key = match RsaSigningKey::new(&PrivateKey(key)) {
            Ok(key) => key,
            Err(_) => {
                return Err(io::Error::new(io::ErrorKind::InvalidData, "Sign Error"));
            }
        };
        let k = CertifiedKey::new(certificate, Arc::new(key));
        Ok(SniCert {
            cert_name: server_name,
            cert: k,
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
        println!("Loading certificate for {}", cert.cert_name);
        sni.add(&cert.cert_name, cert.cert.clone())?;
    }

    let config = rustls::ServerConfig::builder()
        .with_safe_defaults()
        .with_no_client_auth()
        .with_cert_resolver(Arc::new(sni));
    let mut server = TLSServer::new(listener, Arc::new(config));
    println!("Listening on {}", listen);
    loop {
        let connection = server.accept().await;
    }
}