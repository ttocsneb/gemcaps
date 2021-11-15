use serde::Deserialize;
use toml::Value;
use std::{io, sync::Arc};
use tokio::fs;
use async_trait::async_trait;
use regex::Regex;

use crate::gemini;
use crate::pathutil;

pub mod files;

#[derive(Deserialize)]
struct CapsuleConf {
    name: String,
    domain: String,
    rules: Vec<String>,
}


pub struct CapsuleConfig {
    pub name: String,
    pub domain: String,
    pub rules: Vec<Regex>,
}

impl CapsuleConfig {
    fn from(conf: CapsuleConf) -> Result<Self, regex::Error> {
        let mut rules = Vec::new();
        for rule in conf.rules {
            rules.push(Regex::new(&rule)?)
        }
        Ok(CapsuleConfig {
            name: conf.name,
            domain: conf.domain,
            rules,
        })
    }
    pub fn test(&self, domain: &str, path: &str) -> bool {
        if domain != self.domain {
            return false;
        }
        for rule in &self.rules {
            if rule.is_match(path) {
                return true;
            }
        }
        true
    }
}

#[async_trait]
pub trait Capsule : Send + Sync + 'static {
    fn get_capsule(&self) -> &CapsuleConfig;
    fn test(&self, domain: &str, path: &str) -> bool;
    async fn serve(&self, request: &gemini::Request) -> Result<String, Box<dyn std::error::Error>>;
}

pub trait Loader {
    /// Check if the loader is willing to load the config
    fn can_load(&self, value: &Value) -> bool;
    /// Load the config file
    fn load(&self, path: &str, conf: CapsuleConfig, value: Value) -> io::Result<Arc<dyn Capsule>>;
}

/// Load a capsule given a config path and a list of loaders
pub async fn load_capsule(path: &str, loaders: &[Arc<dyn Loader>]) -> Result<Arc<dyn Capsule>, Box<dyn std::error::Error>> {
    let value = fs::read_to_string(path).await?.parse::<Value>()?;
    let conf: CapsuleConf = value.clone().try_into()?;
    let config = CapsuleConfig::from(conf)?;

    for loader in loaders {
        if loader.as_ref().can_load(&value) {
            return Ok(loader.load(path, config, value)?);
        }
    }
    return Err(Box::new(io::Error::new(
        io::ErrorKind::InvalidData, 
        format!("Could not find a willing loader for '{}'", config.name)
    )));
}

/// Load all capsules in a directory using the given list of loaders
pub async fn load_capsules(dir: &str, loaders: &[Arc<dyn Loader>]) -> Result<Vec<Arc<dyn Capsule>>, Box<dyn std::error::Error>> {
    let mut dirs = match fs::read_dir(dir).await {
        Ok(dirs) => dirs,
        Err(err) => {
            if err.kind() != io::ErrorKind::NotFound {
                return Err(Box::new(err));
            } 
            fs::create_dir_all(dir).await?;
            fs::read_dir(dir).await?
        }
    };
    let mut capsules = Vec::new();
    while let Some(ent) = dirs.next_entry().await? {
        if let Some(p) = ent.path().to_str() {
            let (_name, ext) = pathutil::splitext(p);
            if ext.to_ascii_lowercase() != ".toml" {
                eprintln!("Skipping '{}' because it is not a toml file", p);
                continue;
            }
            capsules.push(load_capsule(p, loaders).await?);
        }
    }
    if capsules.len() == 0 {
        return Err(Box::new(io::Error::new(
            io::ErrorKind::NotFound, 
            "Could not find any capsules to load"
        )));
    }
    Ok(capsules)
}
