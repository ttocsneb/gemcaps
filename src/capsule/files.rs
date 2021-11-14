use serde::Deserialize;
use toml::Value;
use std::{io, sync::Arc};
use tokio::fs;
use async_trait::async_trait;

use crate::capsule::{CapsuleConfig, Capsule, Loader};
use crate::gemini;
use crate::pathutil;

#[derive(Deserialize)]
pub struct FileConfig {
    pub directory: String,
    pub extensions: Vec<String>,
    pub serve_folders: bool,
}

pub struct FileCapsule {
    capsule: CapsuleConfig,
    directory: String,
    extensions: Vec<String>,
    serve_folders: bool,
}

#[async_trait]
impl Capsule for FileCapsule {
    fn get_capsule(&self) -> &CapsuleConfig {
        &self.capsule
    }
    fn test(&self, _domain: &str, path: &str) -> bool {
        if self.extensions.is_empty() {
            return true;
        }
        let name = pathutil::basename(path);
        let (_name, ext) = pathutil::splitext(&name);
        if !ext.is_empty() && !self.extensions.contains(&ext) {
            return false;
        }
        true
    }
    async fn serve(&self, request: &gemini::Request) -> Result<String, Box<dyn std::error::Error>> {
        let _file = pathutil::join(&self.directory, &request.path);

        Ok(String::from("20 text/gemini\r\nHello World!\n"))
    }
}

pub struct FileConfigLoader;

impl Loader for FileConfigLoader {
    fn can_load(&self, value: &Value) -> bool {
        if let Some(table) = value.as_table() {
            if !table.contains_key("files") {
                return false;
            }
            if table["files"].is_table() {
                return true;
            }
        }
        false
    }

    fn load(&self, conf: CapsuleConfig, value: Value) -> io::Result<Arc<dyn Capsule>> {
        let files: FileConfig = value["files"].clone().try_into()?;
        Ok(Arc::new(FileCapsule {
            capsule: conf,
            directory: files.directory,
            extensions: files.extensions,
            serve_folders: files.serve_folders,
        }))
    }
}