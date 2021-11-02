use std::fs;
use std::io;
use std::iter;
use rustls_pemfile::{Item, read_one};



fn read_pem(filename: &str) -> io::Result<io::BufReader<fs::File>> {
    let f = fs::File::open(filename)?;
    Ok(io::BufReader::new(f))
}

pub fn read_cert(filename: &str) -> io::Result<Vec<u8>> {
    println!("Reading {}", filename);
    let mut pem = read_pem(filename)?;
    for item in iter::from_fn(|| read_one(&mut pem).transpose()) {
        match item? {
            Item::X509Certificate(cert) => { return Ok(cert); }
            Item::RSAKey(_) => {}
            Item::PKCS8Key(_) => {}
        }
    }
    Err(io::Error::new(io::ErrorKind::InvalidData, "No certificate data was found"))
}

pub fn read_key(filename: &str) -> io::Result<Vec<u8>> {
    println!("Reading {}", filename);
    let mut pem = read_pem(filename)?;
    for item in iter::from_fn(|| read_one(&mut pem).transpose()) {
        match item? {
            Item::X509Certificate(_) => {}
            Item::RSAKey(key) => { return Ok(key); }
            Item::PKCS8Key(key) => { return Ok(key); }
        }
    }
    Err(io::Error::new(io::ErrorKind::InvalidData, "No key data was found"))
}
