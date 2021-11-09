use serde::Deserialize;
use toml::Value;
use std::{io, sync::Arc};
use tokio::fs;
use async_trait::async_trait;

use crate::gemini;

mod files;


#[derive(Deserialize)]
pub struct CapsuleConfig {
    pub name: String,
    pub domain: String,
    pub rules: Vec<String>,
}

impl CapsuleConfig {
    pub fn test(&self, domain: &str, path: &str) -> bool {
        todo!()
    }
}

#[async_trait]
pub trait Capsule {
    fn get_capsule(&self) -> &CapsuleConfig;
    fn test(&self, domain: &str, path: &str) -> bool;
    async fn serve(&self, request: &gemini::Request);
}

pub trait Loader {
    /// Check if the loader is willing to load the config
    fn can_load(&self, value: &Value) -> bool;
    /// Load the config file
    fn load(&self, conf: CapsuleConfig, value: Value) -> io::Result<Arc<dyn Capsule>>;
}

/// Load a capsule given a config path and a list of loaders
pub async fn load_capsule(path: &str, loaders: &[Arc<dyn Loader>]) -> io::Result<Arc<dyn Capsule>> {
    let value = fs::read_to_string(path).await?.parse::<Value>()?;
    let conf: CapsuleConfig = value.clone().try_into()?;

    for loader in loaders {
        if loader.as_ref().can_load(&value) {
            return loader.load(conf, value);
        }
    }
    return Err(io::Error::new(
        io::ErrorKind::InvalidData, 
        format!("Could not find a willing loader for '{}'", conf.name)
    ));
}
