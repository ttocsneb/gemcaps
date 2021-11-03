use std::fs;
use std::io;
use std::iter;
use rustls::Certificate;
use rustls::PrivateKey;
use rustls_pemfile::{Item, read_one, certs};



fn read_pem(filename: &str) -> io::Result<io::BufReader<fs::File>> {
    let f = fs::File::open(filename)?;
    Ok(io::BufReader::new(f))
}

pub fn read_cert(filename: &str) -> io::Result<Vec<Certificate>> {
    let mut pem = read_pem(filename)?;
    Ok(certs(&mut pem)?
        .iter()
        .map(|v| Certificate(v.clone()))
        .collect())
}

pub fn read_key(filename: &str) -> io::Result<PrivateKey> {
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
