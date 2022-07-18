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


#[derive(Debug, Clone, Default)]
pub struct Logger {
    name: String,
    group: Option<String>,
    topic: Option<String>,
    access_file: Option<PathBuf>,
    error_file: Option<PathBuf>,
    access: Option<Arc<Mutex<File>>>,
    error: Option<Arc<Mutex<File>>>,
}

#[allow(dead_code)]
impl Logger {
    pub fn new(name: impl Into<String>) -> Self {
        Self { name: name.into(), ..Default::default() }
    }

    pub fn set_group(&mut self, group: impl Into<String>) {
        self.group = Some(group.into());
    }

    pub fn as_group(&self, group: impl Into<String>) -> Self {
        let mut copy = self.clone();
        copy.set_group(group);
        copy
    }

    pub fn set_topic(&mut self, topic: impl Into<String>) {
        self.topic = Some(topic.into());
    }

    pub fn as_topic(&self, topic: impl Into<String>) -> Self {
        let mut copy = self.clone();
        copy.set_topic(topic);
        copy
    }

    fn format(&self, action: &str, message: impl AsRef<str>) -> String {
        if let Some(group) = &self.group {
            if let Some(topic) = &self.topic {
                format!("{} [{}|{}] {} - {}", action, self.name, group, topic, message.as_ref())
            } else {
                format!("{} [{}|{}] {}", action, self.name, group, message.as_ref())
            }
        } else {
            if let Some(topic) = &self.topic {
                format!("{} [{}] {} - {}", action, self.name, topic, message.as_ref())
            } else {
                format!("{} [{}] {}", action, self.name, message.as_ref())
            }
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

    pub async fn set_access(&mut self, access: impl Into<PathBuf>) -> io::Result<()> {
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

    pub async fn set_error(&mut self, error: impl Into<PathBuf>) -> io::Result<()> {
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

    pub async fn set_logs(&mut self, access: Option<PathBuf>, error: Option<PathBuf>) -> io::Result<()> {
        if let Some(access) = access {
            self.set_access(access).await?;
        }
        if let Some(error) = error {
            self.set_error(error).await?;
        }
        Ok(())
    }

    pub async fn as_logs(&self, access: Option<PathBuf>, error: Option<PathBuf>) -> io::Result<Self> {
        let mut copy = self.clone();
        copy.set_logs(access, error).await?;
        Ok(copy)
    }
}
