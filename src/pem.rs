use std::fs;
use std::io;
use std::iter;
use std::path::Path;
use tokio_rustls::rustls::{Certificate, PrivateKey};
use rustls_pemfile::{Item, read_one, certs};



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
            Item::X509Certificate(_) => {}
            Item::RSAKey(key) => { return Ok(PrivateKey(key)); }
            Item::PKCS8Key(key) => { return Ok(PrivateKey(key)); }
        }
    }
    Err(io::Error::new(io::ErrorKind::InvalidData, "No key data was found"))
}
