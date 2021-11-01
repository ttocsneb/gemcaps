use tokio::fs;
use std::error::Error;
use std::io::ErrorKind;
use serde::{Serialize, Deserialize};

const SETTINGS: &str = "conf.toml";
const DEFAULT_LISTEN: &str = "0.0.0.0:1965";


#[derive(Serialize, Deserialize)]
pub struct Settings {
    pub listen: String,
}


pub async fn load_settings() -> Result<Settings, Box<dyn Error>> {
    let res = match fs::read_to_string(SETTINGS).await {
        Err(e) => {
            if e.kind() != ErrorKind::NotFound {
                return Err(Box::new(e));
            }
            // If the settings don't exist, create the default settings
            let defaults = Settings {
                listen: String::from(DEFAULT_LISTEN),
            };
            save_settings(&defaults).await?;
            return Ok(defaults);
        },
        Ok(s) => s,
    };
    let sett: Settings = toml::from_str(&res)?;
    Ok(sett)
}

pub async fn save_settings(sett: &Settings) -> Result<(), Box<dyn Error>> {
    let content = toml::to_string(&sett)?;
    fs::write(SETTINGS, content).await?;
    Ok(())
}