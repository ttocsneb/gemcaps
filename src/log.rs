use std::{path::{PathBuf, Path}, io, sync::{Arc}};

use tokio::{fs::{File, OpenOptions, self}, io::AsyncWriteExt, sync::Mutex};


async fn open_log_file(path: impl AsRef<Path>) -> io::Result<File> {
    let path = path.as_ref();
    if !path.exists() {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).await.map_err(
                |err| io::Error::new(err.kind(), format!("Unable to open {:?}: {}", path, err))
            )?;
        }
    }
    OpenOptions::new()
        .create(true)
        .append(true)
        .write(true)
        .open(path).await.map_err(
            |err| io::Error::new(err.kind(), format!("Unable to open {:?}: {}", path, err))
        )

}


pub struct LoggerBuilder {
    name: String,
    group: Option<String>,
    access_file: Option<PathBuf>,
    error_file: Option<PathBuf>,
}

impl LoggerBuilder {
    pub fn new(name: impl Into<String>) -> Self {
        Self { name: name.into(), group: None, access_file: None, error_file: None }
    }

    pub fn name(mut self, name: impl Into<String>) -> Self {
        self.name = name.into();
        self
    }

    pub fn group(mut self, group: impl Into<String>) -> Self {
        self.group = Some(group.into());
        self
    }

    pub fn access(mut self, file: impl Into<PathBuf>) -> Self {
        self.access_file = Some(file.into());
        self
    }

    pub fn set_access(mut self, file: Option<PathBuf>) -> Self {
        self.access_file = file;
        self
    }

    pub fn error(mut self, file: impl Into<PathBuf>) -> Self {
        self.error_file = Some(file.into());
        self
    }
    
    pub fn set_error(mut self, file: Option<PathBuf>) -> Self {
        self.error_file = file;
        self
    }

    pub async fn open(self) -> io::Result<Logger> {
        let access = match &self.access_file {
            Some(f) => {
                Some(Arc::new(Mutex::new(open_log_file(f).await?)))
            },
            None => None,
        };
        // Prevent the same file to be open by two separate file objects
        let error = if self.access_file == self.error_file {
            access.clone()
        } else {
            match &self.error_file {
                Some(f) => {
                    Some(Arc::new(Mutex::new(open_log_file(f).await?)))
                },
                None => None,
            }
        };
        Ok(Logger { 
            name: self.name, 
            group: self.group,
            access,
            error,
            access_file: self.access_file,
            error_file: self.error_file,
        })
    }
}


#[derive(Debug, Clone)]
pub struct Logger {
    name: String,
    group: Option<String>,
    access_file: Option<PathBuf>,
    error_file: Option<PathBuf>,
    access: Option<Arc<Mutex<File>>>,
    error: Option<Arc<Mutex<File>>>,
}

impl Logger {
    #[inline]
    pub fn builder(name: impl Into<String>) -> LoggerBuilder {
        LoggerBuilder::new(name)
    }

    pub fn option(&mut self, group: impl Into<String>) {
        self.group = Some(group.into());
    }

    pub fn as_option(&self, group: impl Into<String>) -> Self {
        let mut copy = self.clone();
        copy.option(group);
        copy
    }

    fn format(&self, action: &str, message: impl AsRef<str>) -> String {
        if let Some(group) = &self.group {
            format!("{} [{}|{}] {}", action, self.name, group, message.as_ref())
        } else {
            format!("{} [{}] {}", action, self.name, message.as_ref())
        }
    }

    pub async fn access(&mut self, message: impl AsRef<str>) {
        let message = self.format("ACCES", message);
        if let Some(access) = &self.access {
            let mut access = access.lock().await;
            let now = chrono::offset::Local::now();
            match access.write_all(format!("{:?} {}\n", now, message).as_bytes()).await {
                Ok(_) => {},
                Err(err) => {
                    let message = self.format("ERROR", err.to_string());
                    eprintln!("{}", message);
                }
            };
        }
        println!("{}", message);
    }

    pub async fn error(&mut self, message: impl AsRef<str>) {
        let message = self.format("ERROR", message);
        if let Some(error) = &self.error {
            let mut error = error.lock().await;
            let now = chrono::offset::Local::now();
            match error.write_all(format!("{:?} {}\n", now, message).as_bytes()).await {
                Ok(_) => {},
                Err(err) => {
                    let message = self.format("ERROR", err.to_string());
                    eprintln!("{}", message);
                }
            }
        }
        eprintln!("{}", message);
    }
    
    pub fn info(&mut self, message: impl AsRef<str>) {
        println!("{}", self.format("INFO ", message));
    }

    pub async fn replace_access(&mut self, access: impl Into<PathBuf>) -> io::Result<()> {
        let access = access.into();
        // Prevent the same file to be open by two separate file objects
        if let Some(error_file) = &self.error_file {
            if access == *error_file {
                self.access = self.error.clone();
                self.access_file = Some(access);
                return Ok(());
            }
        }
        if let Some(access_file) = &self.access_file {
            if access == *access_file {
                return Ok(())
            }
        }
        self.access = Some(Arc::new(Mutex::new(open_log_file(&access).await?)));
        self.access_file = Some(access);

        Ok(())
    }

    pub async fn replace_error(&mut self, error: impl Into<PathBuf>) -> io::Result<()> {
        let error = error.into();
        // Prevent the same file to be open by two separate file objects
        if let Some(access_file) = &self.access_file {
            if error == *access_file {
                self.error = self.access.clone();
                self.error_file = Some(error);
                return Ok(());
            }
        }
        if let Some(error_file) = &self.error_file {
            if error == *error_file {
                return Ok(())
            }
        }
        self.error = Some(Arc::new(Mutex::new(open_log_file(&error).await?)));
        self.error_file = Some(error);

        Ok(())
    }

    pub async fn replace_logs(&mut self, access: Option<PathBuf>, error: Option<PathBuf>) -> io::Result<()> {
        if let Some(access) = access {
            self.replace_access(access).await?;
        }
        if let Some(error) = error {
            self.replace_error(error).await?;
        }
        Ok(())
    }

    pub async fn as_replace_logs(&self, access: Option<PathBuf>, error: Option<PathBuf>) -> io::Result<Self> {
        let mut copy = self.clone();
        copy.replace_logs(access, error).await?;
        Ok(copy)
    }
    
    // pub fn access(&)
}