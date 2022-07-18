use std::io;

use rustls::{server::{self, Accepted}, Error};
use tokio::{net::TcpStream, io::AsyncReadExt};

use super::buffer::Buffer;

pub struct Acceptor {
    pub buffer: Buffer,
    acceptor: server::Acceptor,
}

impl Acceptor {
    pub fn new() -> Result<Self, Error> {
        Ok(Self {
            buffer: Buffer::with_capacity(1024),
            acceptor: server::Acceptor::new()?,
        })
    }
    async fn read_tls(&mut self, rd: &mut TcpStream) -> Result<usize, io::Error> {
        rd.read_buf(&mut self.buffer.buf).await?;
        self.acceptor.read_tls(&mut self.buffer)
    }

    pub async fn accept(&mut self, rd: &mut TcpStream) -> Result<Accepted, io::Error> {
        loop {
            if self.acceptor.wants_read() {
                self.read_tls(rd).await?;
            }
            match self.acceptor.accept() {
                Ok(Some(accepted)) => {
                    return Ok(accepted);
                },
                Ok(None) => {},
                Err(err) => {
                    return Err(io::Error::new(io::ErrorKind::InvalidData, err));
                }
            }
        }
    }
}