use tokio::fs;
use std::collections::HashMap;
use std::error::Error;
use std::io::ErrorKind;
use serde::{Serialize, Deserialize};

use crate::pathutil;

const SETTINGS: &str = "conf.toml";
const DEFAULT_LISTEN: &str = "0.0.0.0:1965";
const DEFAULT_CAPSULES: &str = "capsules";
const DEFAULT_CACHE: f32 = 60.0;


#[derive(Serialize, Deserialize)]
struct SettingsConf {
    listen: Option<String>,
    capsules: Option<String>,
    cache: Option<f32>,
    certificates: HashMap<String, Cert>,
}

pub struct Settings {
    pub config_dir: String,
    pub listen: String,
    pub capsules: String,
    pub cache: f32,
    pub certificates: HashMap<String, Cert>,
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
                capsules: String::from(DEFAULT_CAPSULES),
                config_dir: String::from(dir),
                cache: DEFAULT_CACHE,
                certificates: HashMap::new(),
            };
            save_settings(&defaults).await?;
            return Ok(defaults);
        },
        Ok(s) => s,
    };
    let sett: SettingsConf = toml::from_str(&res)?;
    Ok(Settings {
        listen: sett.listen.unwrap_or_else(|| DEFAULT_LISTEN.to_string()),
        capsules: sett.capsules.unwrap_or_else(|| DEFAULT_CAPSULES.to_string()),
        config_dir: String::from(dir),
        cache: sett.cache.unwrap_or_else(|| DEFAULT_CACHE),
        certificates: sett.certificates,
    })
}

pub async fn save_settings(sett: &Settings) -> Result<(), Box<dyn Error>> {
    let conf = SettingsConf {
        listen: Some(sett.listen.to_owned()),
        certificates: sett.certificates.to_owned(),
        capsules: Some(sett.capsules.to_owned()),
        cache: Some(sett.cache),
    };
    let content = toml::to_string(&conf)?;
    fs::write(pathutil::join(&sett.config_dir, SETTINGS), content).await?;
    Ok(())
}