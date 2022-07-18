use serde::Deserialize;
use toml::Value;
use std::fmt::Display;
use std::path::Path;
use std::{io, sync::Arc};
use tokio::fs;
use async_trait::async_trait;
use regex::Regex;

use crate::gemini;
use crate::pathutil;

pub mod files;
pub mod file;
pub mod redirect;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CapsuleResponse {
    Input { prompt: String },
    InputSensitive { prompt: String },
    Success {
        meta: String,
        body: String,
    },
    RedirectTemp { redirect: String },
    RedirectPerm { redirect: String },
    FailureTemp { message: String },
    ServerUnavail { message: String },
    CgiError { message: String },
    ProxyError { message: String },
    SlowDown { message: String },
    FailurePerm { message: String },
    NotFound { message: String },
    Gone { message: String },
    ProxyRefused { message: String },
    BadRequest { message: String },
    CertRequired { message: String },
    CertNotAuthorized { message: String },
    CertInvalid { message: String },
}

impl CapsuleResponse {
    pub fn input(prompt: impl Into<String>) -> Self {
        Self::Input { prompt: prompt.into() }
    }

    pub fn input_sensitive(prompt: impl Into<String>) -> Self {
        Self::InputSensitive { prompt: prompt.into() }
    }
    
    pub fn success(meta: impl Into<String>, body: impl Into<String>) -> Self {
        Self::Success { meta: meta.into(), body: body.into() }
    }
    
    pub fn redirect_temp(redirect: impl Into<String>) -> Self {
        Self::RedirectTemp { redirect: redirect.into() }
    }
    
    pub fn redirect_perm(redirect: impl Into<String>) -> Self {
        Self::RedirectPerm { redirect: redirect.into() }
    }
    
    pub fn failure_temp(message: impl Into<String>) -> Self {
        Self::FailureTemp { message: message.into() }
    }

    pub fn server_unavail(message: impl Into<String>) -> Self {
        Self::ServerUnavail { message: message.into() }
    }

    pub fn cgi_error(message: impl Into<String>) -> Self {
        Self::CgiError { message: message.into() }
    }

    pub fn proxy_error(message: impl Into<String>) -> Self {
        Self::ProxyError { message: message.into() }
    }

    pub fn slow_down(message: impl Into<String>) -> Self {
        Self::SlowDown { message: message.into() }
    }

    pub fn failure_perm(message: impl Into<String>) -> Self {
        Self::FailurePerm { message: message.into() }
    }

    pub fn not_found(message: impl Into<String>) -> Self {
        Self::NotFound { message: message.into() }
    }

    pub fn gone(message: impl Into<String>) -> Self {
        Self::Gone { message: message.into() }
    }

    pub fn proxy_refused(message: impl Into<String>) -> Self {
        Self::ProxyRefused { message: message.into() }
    }

    pub fn bad_request(message: impl Into<String>) -> Self {
        Self::BadRequest { message: message.into() }
    }

    pub fn cert_required(message: impl Into<String>) -> Self {
        Self::CertRequired { message: message.into() }
    }

    pub fn cert_not_authorized(message: impl Into<String>) -> Self {
        Self::CertNotAuthorized { message: message.into() }
    }

    pub fn cert_invalid(message: impl Into<String>) -> Self {
        Self::CertInvalid { message: message.into() }
    }

    pub fn meta(&self) -> &str {
        match self {
            CapsuleResponse::Input { prompt } => &prompt,
            CapsuleResponse::InputSensitive { prompt } => &prompt,
            CapsuleResponse::Success { meta, body: _ } => &meta,
            CapsuleResponse::RedirectTemp { redirect } => &redirect,
            CapsuleResponse::RedirectPerm { redirect } => &redirect,
            CapsuleResponse::FailureTemp { message } => &message,
            CapsuleResponse::ServerUnavail { message } => &message,
            CapsuleResponse::CgiError { message } => &message,
            CapsuleResponse::ProxyError { message } => &message,
            CapsuleResponse::SlowDown { message } => &message,
            CapsuleResponse::FailurePerm { message } => &message,
            CapsuleResponse::NotFound { message } => &message,
            CapsuleResponse::Gone { message } => &message,
            CapsuleResponse::ProxyRefused { message } => &message,
            CapsuleResponse::BadRequest { message } => &message,
            CapsuleResponse::CertRequired { message } => &message,
            CapsuleResponse::CertNotAuthorized { message } => &message,
            CapsuleResponse::CertInvalid { message } => &message,
        }
    }

    pub fn code(&self) -> u8 {
        match self {
            CapsuleResponse::Input { prompt: _ } => 10,
            CapsuleResponse::InputSensitive { prompt: _ } => 11,
            CapsuleResponse::Success { meta: _, body: _ } => 20,
            CapsuleResponse::RedirectTemp { redirect: _ } => 30,
            CapsuleResponse::RedirectPerm { redirect: _ } => 31,
            CapsuleResponse::FailureTemp { message: _ } => 40,
            CapsuleResponse::ServerUnavail { message: _ } => 41,
            CapsuleResponse::CgiError { message: _ } => 42,
            CapsuleResponse::ProxyError { message: _ } => 43,
            CapsuleResponse::SlowDown { message: _ } => 44,
            CapsuleResponse::FailurePerm { message: _ } => 50,
            CapsuleResponse::NotFound { message: _ } => 51,
            CapsuleResponse::Gone { message: _ } => 52,
            CapsuleResponse::ProxyRefused { message: _ } => 53,
            CapsuleResponse::BadRequest { message: _ } => 59,
            CapsuleResponse::CertRequired { message: _ } => 60,
            CapsuleResponse::CertNotAuthorized { message: _ } => 61,
            CapsuleResponse::CertInvalid { message: _ } => 62,
        }
    }

    pub fn name(&self) -> &'static str {
        match self {
            CapsuleResponse::Input { prompt: _ } => "input",
            CapsuleResponse::InputSensitive { prompt: _ } => "sensitive input",
            CapsuleResponse::Success { meta: _, body: _ } => "success",
            CapsuleResponse::RedirectTemp { redirect: _ } => "temporary redirect",
            CapsuleResponse::RedirectPerm { redirect: _ } => "permanent redirect",
            CapsuleResponse::FailureTemp { message: _ } => "temporary failure",
            CapsuleResponse::ServerUnavail { message: _ } => "server unavailable",
            CapsuleResponse::CgiError { message: _ } => "cgi error",
            CapsuleResponse::ProxyError { message: _ } => "proxy error",
            CapsuleResponse::SlowDown { message: _ } => "slow down",
            CapsuleResponse::FailurePerm { message: _ } => "permanent failure",
            CapsuleResponse::NotFound { message: _ } => "not found",
            CapsuleResponse::Gone { message: _ } => "gone",
            CapsuleResponse::ProxyRefused { message: _ } => "proxy refused",
            CapsuleResponse::BadRequest { message: _ } => "bad request",
            CapsuleResponse::CertRequired { message: _ } => "certificate required",
            CapsuleResponse::CertNotAuthorized { message: _ } => "certificate not authorized",
            CapsuleResponse::CertInvalid { message: _ } => "certificate not valid",
        }
    }

    pub fn header(&self) -> String {
        format!("{} {}\r\n", self.code(), self.meta())
    }

    pub fn body(&self) -> Option<&str> {
        match &self {
            CapsuleResponse::Success { meta: _, body } => Some(body),
            _ => None
        }
    }

    pub fn info(&self) -> String {
        format!("{} ({}) {}", self.code(), self.name(), self.meta())
    }
}

impl Display for CapsuleResponse {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.header())?;
        if let Some(body) = self.body() {
            f.write_str(body)?;
        };
        Ok(())
    }
}


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
    /// Get the general capsule settings for this capsule
    fn get_capsule(&self) -> &CapsuleConfig;
    /// Test whether this capsule is willing to handle the given request
    fn test(&self, domain: &str, path: &str) -> bool;
    /// Serve a client's request
    /// 
    /// This should return a string of the full response including headers e.g.
    /// "20 text/gemini\r\nHello World" and the time to cache this response.
    /// 
    /// If the cache time is 0.0, then the default cache time will be used for this response.
    /// If No cache time is given, then no cache will be made.
    /// 
    async fn serve(&self, request: &gemini::Request) -> Result<(String, Option<f32>), Box<dyn std::error::Error>>;
}

pub trait Loader {
    /// Check if the loader is willing to load the config
    fn can_load(&self, value: &Value) -> bool;
    /// Load the config file
    fn load(&self, path: &Path, conf: CapsuleConfig, value: Value) -> io::Result<Arc<dyn Capsule>>;
}

/// Load a capsule given a config path and a list of loaders
pub async fn load_capsule(path: impl AsRef<Path>, loaders: &[Arc<dyn Loader>]) -> Result<Arc<dyn Capsule>, Box<dyn std::error::Error>> {
    let path = path.as_ref();
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
pub async fn load_capsules(dir: impl AsRef<Path>, loaders: &[Arc<dyn Loader>]) -> Result<Vec<Arc<dyn Capsule>>, Box<dyn std::error::Error>> {
    let dir = dir.as_ref();
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
