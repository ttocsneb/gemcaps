use serde::Deserialize;
use toml::Value;
use std::{io, sync::Arc};
use async_trait::async_trait;

use crate::capsule::{CapsuleConfig, Capsule, Loader};
use crate::gemini;

#[derive(Deserialize)]
pub struct FileConfig {
    pub directory: String,
    pub extensions: Vec<String>,
    pub serve_folders: bool,
}

pub struct FileCapsule {
    capsule: CapsuleConfig,
    files: FileConfig,
}

#[async_trait]
impl Capsule for FileCapsule {
    fn get_capsule(&self) -> &CapsuleConfig {
        &self.capsule
    }
    fn test(&self, domain: &str, path: &str) -> bool {
        true
    }
    async fn serve(&self, request: &gemini::Request) {

    }
}

pub struct FileConfigLoader;

impl Loader for FileConfigLoader {
    fn can_load(&self, value: &Value) -> bool {
        if !value.is_table() {
            return false;
        }
        value["files"].as_table() != None
    }

    fn load(&self, conf: CapsuleConfig, value: Value) -> io::Result<Arc<dyn Capsule>> {
        let files: FileConfig = value["files"].clone().try_into()?;
        Ok(Arc::new(FileCapsule {
            capsule: conf,
            files,
        }))
    }
}