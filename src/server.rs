extern crate tokio;

use tokio::net::{TcpListener, TcpStream};
use std::{collections::HashMap, io, sync::Arc};

// Using https://github.com/rustls/rustls/blob/main/rustls-mio/examples/tlsserver.rs as a tutorial

struct TLSServer {
    server: TcpListener,
    connections: HashMap<usize, TLSConnection>,
    next_id: usize,
    tls_config: Arc<rustls::ServerConfig>,
}

impl TLSServer {
    fn new(server: TcpListener, cfg: Arc<rustls::ServerConfig>) -> Self {
        TLSServer {
            server,
            connections: HashMap::new(),
            next_id: 2,
            tls_config: cfg
        }
    }

    async fn accept(&mut self) {
        match self.server.accept().await {
            Ok((socket, addr)) => {
                println!("Accepting new connection from {:?}", addr);
                let tls_conn = rustls::ServerConnection::new(Arc::clone(&self.tls_config)).unwrap();
                let token = self.next_id;
                self.next_id += 1;

                let mut connection = TLSConnection::new(socket, token, tls_conn);
                self.connections.insert(token, connection);
            }
            Err(e) => {
                println!("Couldn't accept client: {:?}", e);
            }
        };
    }
}

struct TLSConnection {
    id: usize,
    socket: TcpStream,
    conn: rustls::ServerConnection,
    closing: bool,
    closed: bool,
}

impl TLSConnection {
    fn new(socket: TcpStream, token: usize, conn: rustls::ServerConnection) -> TLSConnection {
        TLSConnection {
            id: token,
            socket,
            conn,
            closing: false,
            closed: false,
        }
    }
}

async fn process_socket(socket: TcpStream) -> io::Result<()> {
    println!("Receiving Connection from {:?}", socket.peer_addr());

    // TODO: Implement rustls

    Ok(())
}


pub async fn serve(listen: &str) -> io::Result<()> {
    let listener = TcpListener::bind(listen).await?;
    println!("Listening on {}", listen);
    loop {
        let (socket, _) = listener.accept().await?;
        process_socket(socket).await?;
    }
}