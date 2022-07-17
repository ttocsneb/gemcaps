use std::io::{self, Write, Read};

use bytes::Buf;
use rustls::{ServerConnection, IoState, Certificate, Writer, Reader};
use tokio::{net::TcpStream, io::{AsyncReadExt, AsyncWriteExt}};


pub struct Connection {
    connection: ServerConnection,
    stream: TcpStream,
}

impl Connection {
    pub fn new(connection: ServerConnection, stream: TcpStream) -> Self {
        Self {
            connection,
            stream,
        }
    }

    pub async fn read_tls(&mut self) -> io::Result<IoState> {
        let mut buf = Vec::with_capacity(1024);
        let read = self.stream.read_buf(&mut buf).await?;
        let tls_read = self.connection.read_tls(&mut buf.reader())?;
        assert_eq!(read, tls_read, "Data read from stream and tls do not match");
        let state = self.connection.process_new_packets().map_err(
            |err| io::Error::new(io::ErrorKind::InvalidData, err)
        )?;
        Ok(state)
    }

    pub async fn write_tls(&mut self) -> io::Result<usize> {
        let mut buf = Vec::with_capacity(1024);
        let written = self.connection.write_tls(&mut buf)?;
        self.stream.write_all(&buf).await?;
        Ok(written)
    }

    pub async fn complete_io(&mut self) -> io::Result<()> {
        let mut changed_state = true;
        while changed_state {
            changed_state = false;
            if self.wants_read() {
                self.read_tls().await?;
                changed_state = true;
            }
            if self.wants_write() {
                self.write_tls().await?;
                changed_state = true;
            }
        }
        Ok(())
    }

    pub async fn send(&mut self, message: impl AsRef<str>) -> io::Result<()> {
        self.writer().write_all(message.as_ref().as_bytes())?;
        self.write_tls().await?;
        Ok(())
    }

    pub async fn receive(&mut self, count: usize) -> io::Result<String> {
        if self.wants_read() {
            self.read_tls().await?;
        }
        let mut buf = vec![0; count];
        let count = self.reader().read(&mut buf)?;
        String::from_utf8(buf[0..count].to_vec()).map_err(
            |err| io::Error::new(io::ErrorKind::InvalidData, err)
        )
    }

    #[inline]
    pub fn wants_read(&self) -> bool {
        self.connection.wants_read()
    }

    #[inline]
    pub fn wants_write(&self) -> bool {
        self.connection.wants_write()
    }

    #[inline]
    pub fn wants_io(&self) -> bool {
        self.wants_read() || self.wants_write()
    }

    #[inline]
    pub fn alpn_protocol(&self) -> Option<&[u8]> {
        self.connection.alpn_protocol()
    }

    #[inline]
    pub fn is_handshaking(&self) -> bool {
        self.connection.is_handshaking()
    }

    #[inline]
    pub fn peer_certificates(&self) -> Option<&[Certificate]> {
        self.connection.peer_certificates()
    }
    
    #[inline]
    pub fn send_close_notify(&mut self) {
        self.connection.send_close_notify()
    }

    #[inline]
    pub fn writer(&mut self) -> Writer {
        self.connection.writer()
    }

    #[inline]
    pub fn reader(&mut self) -> Reader {
        self.connection.reader()
    }

}