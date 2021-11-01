
mod settings;
mod files;
mod server;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>>  {

    let sett = settings::load_settings().await?;

    server::serve(&sett.listen).await?;

    Ok(())
}
