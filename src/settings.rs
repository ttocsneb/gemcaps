use tokio::fs;
use std::collections::HashMap;
use std::error::Error;
use std::io::ErrorKind;
use serde::{Serialize, Deserialize};

use crate::pathutil;

const SETTINGS: &str = "conf.toml";
const DEFAULT_LISTEN: &str = "0.0.0.0:1965";
const DEFAULT_CAPSULES: &str = "capsules";


#[derive(Serialize, Deserialize)]
struct SettingsConf {
    listen: String,
    certificates: HashMap<String, Cert>,
    capsules: Option<String>,
}

pub struct Settings {
    pub listen: String,
    pub certificates: HashMap<String, Cert>,
    pub capsules: String,
    pub config_dir: String,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct Cert {
    pub cert: String,
    pub key: String,
}


pub async fn load_settings(dir: &str) -> Result<Settings, Box<dyn Error>> {
    let res = match fs::read_to_string(pathutil::join(dir, SETTINGS)).await {
        Err(e) => {
            if e.kind() != ErrorKind::NotFound {
                return Err(Box::new(e));
            }
            // If the settings don't exist, create the default settings
            let defaults = Settings {
                listen: String::from(DEFAULT_LISTEN),
                certificates: HashMap::new(),
                capsules: String::from(DEFAULT_CAPSULES),
                config_dir: String::from(dir),
            };
            save_settings(&defaults).await?;
            return Ok(defaults);
        },
        Ok(s) => s,
    };
    let sett: SettingsConf = toml::from_str(&res)?;
    Ok(Settings {
        listen: sett.listen,
        certificates: sett.certificates,
        capsules: sett.capsules.unwrap_or_else(|| DEFAULT_CAPSULES.to_string()),
        config_dir: String::from(dir),
    })
}

pub async fn save_settings(sett: &Settings) -> Result<(), Box<dyn Error>> {
    let conf = SettingsConf {
        listen: sett.listen.to_owned(),
        certificates: sett.certificates.to_owned(),
        capsules: Some(sett.capsules.to_owned()),
    };
    let content = toml::to_string(&conf)?;
    fs::write(pathutil::join(&sett.config_dir, SETTINGS), content).await?;
    Ok(())
}