use serde::Deserialize;
use toml::Value;
use std::path::{Path, PathBuf};
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
    pub do_cache: Option<bool>,
    pub cache: Option<f32>,
}

pub struct FileCapsule {
    capsule: CapsuleConfig,
    directory: PathBuf,
    extensions: Vec<String>,
    serve_folders: bool,
    cache: Option<f32>,
}

impl FileCapsule {
    /// Read the file to string
    /// 
    /// file is the file on disk.
    /// path is the file on the server.
    async fn read_file(&self, file: impl AsRef<Path>, path: &str) -> io::Result<(String, String)> {
        let file = file.as_ref();
        let mime_type = pathutil::get_mimetype(file).to_string();

        let metadata = fs::metadata(file).await?;
        if metadata.is_file() {
            return Ok((mime_type, fs::read_to_string(file).await?));
        }
        if metadata.is_dir() {
            let mut dirs = fs::read_dir(file).await?;
            let mut folders = Vec::new();
            let mut files = Vec::new();
            // Try to find an index.xyz to read from
            while let Some(ent) = dirs.next_entry().await? {
                if let Some(name) = ent.file_name().to_str() {
                    if !ent.file_type().await?.is_file() {
                        folders.push(format!("{}/", name));
                        continue;
                    }
                    let (filename, ext) = pathutil::splitext(name);
                    if !self.extensions.is_empty() && !self.extensions.contains(&ext.to_ascii_lowercase()) {
                        continue;
                    }
                    files.push(String::from(name));
                    if filename.to_ascii_lowercase() == "index" {
                        let mime_type = pathutil::get_mimetype(ent.path().as_os_str().to_str().unwrap()).to_string();
                        return Ok((mime_type, fs::read_to_string(ent.path()).await?));
                    }
                }
            }
            if !self.serve_folders {
                return Err(io::Error::new(
                    io::ErrorKind::PermissionDenied,
                    "Unable to serve folders"
                ));
            }
            // Return the contents of the folders

            let mut response = format!("# {}\n\n", path);
            folders.sort();
            for item in folders {
                response += &format!("=> {} {}\n", pathutil::join(path, &item), item);
            }
            files.sort();
            for item in files {
                response += &format!("=> {} {}\n", pathutil::join(path, &item), item);
            }
            let mime_type = String::from("text/gemini");
            return Ok((mime_type, response));
        }

        Err(io::Error::new(
            io::ErrorKind::PermissionDenied,
            "This is neither a file or a folder"
        ))
    }
}

#[async_trait]
impl Capsule for FileCapsule {
    fn get_capsule(&self) -> &CapsuleConfig {
        &self.capsule
    }
    fn test(&self, _domain: &str, file: &str) -> bool {
        if self.extensions.is_empty() {
            return true;
        }
        let name = pathutil::basename(file);
        let (_name, ext) = pathutil::splitext(&name);
        if !ext.is_empty() && !self.extensions.iter().any(|x| x == ext) {
            return false;
        }
        if let Err(_) = pathutil::expand(file) {
            return false;
        }
        true
    }
    async fn serve(&self, request: &gemini::Request) -> Result<(String, Option<f32>), Box<dyn std::error::Error>> {
        let path = match request.path.starts_with('/') {
            true => &request.path[1 ..],
            false => &request.path,
        };
        let file = self.directory.join(match pathutil::traversal_safe(path) {
            Ok(path) => path,
            Err(_) => {
                return Ok((format!("51 Permission Denied\r\n"), self.cache));
            }
        });
        println!("file: {:?}", file);

        // Redirect the request to have ending '/' for directories, and no ending '/' for files
        let (_name, ext) = pathutil::splitext(&request.path);
        if ext.is_empty() && !request.path.ends_with("/") {
            return Ok((format!("31 {}/\r\n", request.path), self.cache));
        } else if !ext.is_empty() && request.path.ends_with("/") {
            return Ok((format!("31 {}\r\n", &request.path[0 .. request.path.len() - 1]), self.cache));
        }


        let (response, meta, body) = match self.read_file(&file, &request.path).await {
            Ok((meta, response)) => (20, meta, Some(response)),
            Err(err) => {
                match err.kind() {
                    io::ErrorKind::PermissionDenied => (51, String::from("Permission Denied"), None),
                    _ => (51, String::from("File not found"), None)
                }
            }
        };

        Ok((match body {
            Some(content) => format!("{} {}\r\n{}", response, meta, content),
            None => format!("{} {}\r\n", response, meta),
        }, self.cache))
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

    fn load(&self, path: &Path, conf: CapsuleConfig, value: Value) -> io::Result<Arc<dyn Capsule>> {
        let files: FileConfig = value["files"].clone().try_into()?;
        let base = path.parent().unwrap().parent().unwrap();
        Ok(Arc::new(FileCapsule {
            capsule: conf,
            directory: pathutil::abspath(base, files.directory),
            extensions: files.extensions,
            serve_folders: files.serve_folders,
            cache: match files.do_cache.unwrap_or_else(|| true) {
                true => Some(files.cache.unwrap_or_else(|| 0.0)),
                false => None,
            },
        }))
    }
}