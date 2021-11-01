
mod settings;
mod files;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>>  {

    let sett = settings::load_settings().await?;

    println!("Settings loaded: listen: {}", sett.listen);

    Ok(())
}
