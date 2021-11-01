extern crate tokio;

use tokio::net::{TcpListener, TcpStream};
use std::io;

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