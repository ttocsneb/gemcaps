use std::fs;
use std::io;
use std::iter;
use std::path::Path;
use std::path::PathBuf;
use tokio_rustls::rustls::{Certificate, PrivateKey};
use rustls_pemfile::{Item, read_one, certs};

#[derive(Debug, Clone)]
pub enum Cert {
    Closed(ClosedCert),
    Opened(OpenedCert),
}

impl Cert {
    pub fn new(certfile: impl Into<PathBuf>, keyfile: impl Into<PathBuf>) -> Self {
        Self::Closed(ClosedCert::new(certfile, keyfile))
    }
}

impl Default for Cert {
    fn default() -> Self {
        Self::Closed(ClosedCert::default())
    }
}

#[derive(Debug, Clone, Default)]
pub struct ClosedCert {
    pub certfile: PathBuf,
    pub keyfile: PathBuf,
}

impl ClosedCert {
    pub fn new(certfile: impl Into<PathBuf>, keyfile: impl Into<PathBuf>) -> Self {
        Self { certfile: certfile.into(), keyfile: keyfile.into() }
    }

    pub fn open(self) -> io::Result<OpenedCert> {
        let cert = read_cert(&self.certfile)?;
        let key = read_key(&self.keyfile)?;

        Ok(OpenedCert {
            certfile: self.certfile,
            keyfile: self.keyfile,
            cert,
            key,
        })
    }

    pub fn relative(&mut self, base_dir: impl AsRef<Path>) {
        let base_dir = base_dir.as_ref();
        if self.certfile.is_relative() {
            self.certfile = base_dir.join(&self.certfile);
        }
        if self.keyfile.is_relative() {
            self.keyfile = base_dir.join(&self.keyfile);
        }
    }
}

#[derive(Debug, Clone)]
pub struct OpenedCert {
    pub certfile: PathBuf,
    pub keyfile: PathBuf,
    pub cert: Vec<Certificate>,
    pub key: PrivateKey,
}


fn read_pem(filename: impl AsRef<Path>) -> io::Result<io::BufReader<fs::File>> {
    let f = fs::File::open(filename)?;
    Ok(io::BufReader::new(f))
}

pub fn read_cert(filename: impl AsRef<Path>) -> io::Result<Vec<Certificate>> {
    let mut pem = read_pem(filename)?;
    Ok(certs(&mut pem)?
        .iter()
        .map(|v| Certificate(v.clone()))
        .collect())
}

pub fn read_key(filename: impl AsRef<Path>) -> io::Result<PrivateKey> {
    let mut pem = read_pem(filename)?;
    for item in iter::from_fn(|| read_one(&mut pem).transpose()) {
        match item? {
            Item::RSAKey(key) => { return Ok(PrivateKey(key)); }
            Item::PKCS8Key(key) => { return Ok(PrivateKey(key)); }
            _ => {}
        }
    }
    Err(io::Error::new(io::ErrorKind::InvalidData, "No key data was found"))
}
