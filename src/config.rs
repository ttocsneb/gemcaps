use std::{borrow::Cow, path::PathBuf, fmt::Debug, sync::Arc};

use regex::Regex;
use rustls::ServerConfig;
use serde::Deserialize;

use crate::{error::GemcapsError, pem::Cert, glob::Glob, gemini::Request};

#[derive(Debug, Clone)]
pub struct RedirectConf {
    pub domain_names: Vec<Glob>,
    pub redirect: String,
    pub error_log: Option<PathBuf>,
    pub access_log: Option<PathBuf>,
}

#[derive(Debug, Clone)]
pub struct ProxyConf {
    pub domain_names: Vec<Glob>,
    pub proxy: Request,
    pub error_log: Option<PathBuf>,
    pub access_log: Option<PathBuf>,
    pub rule: Option<Regex>,
    pub substitution: Option<Substitution>,
}

#[derive(Debug, Clone)]
pub struct CGIConf {
    pub domain_names: Vec<Glob>,
    pub cgi_root: PathBuf,
    pub error_log: Option<PathBuf>,
    pub access_log: Option<PathBuf>,
    pub rule: Option<Regex>,
    pub substitution: Option<Substitution>,
}

#[derive(Debug, Clone)]
pub struct FileConf {
    pub domain_names: Vec<Glob>,
    pub file_root: PathBuf,
    pub error_log: Option<PathBuf>,
    pub access_log: Option<PathBuf>,
    pub rule: Option<Regex>,
    pub substitution: Option<Substitution>,
}

#[derive(Debug, Clone)]
pub enum ConfItem {
    Redirect(RedirectConf),
    Proxy(ProxyConf),
    CGI(CGIConf),
    File(FileConf),
}

macro_rules! shared_item {
    ($self:ident.$item:ident) => {
        match &$self {
            ConfItem::Redirect(item) => &item.$item,
            ConfItem::Proxy(item) => &item.$item,
            ConfItem::CGI(item) => &item.$item,
            ConfItem::File(item) => &item.$item,
        }
    };
}

impl ConfItem {
    pub fn domain_names(&self) -> &Vec<Glob> {
        shared_item!(self.domain_names)
    }

    pub fn error_log(&self) -> Option<&PathBuf> {
        shared_item!(self.error_log).as_ref()
    }
    
    pub fn access_log(&self) -> Option<&PathBuf> {
        shared_item!(self.access_log).as_ref()
    }

    pub fn access_log_mut(&mut self) -> Option<&mut PathBuf> {
        match self {
            ConfItem::Redirect(item) => item.access_log.as_mut(),
            ConfItem::Proxy(item) => item.access_log.as_mut(),
            ConfItem::CGI(item) => item.access_log.as_mut(),
            ConfItem::File(item) => item.access_log.as_mut(),
        }
    }

    pub fn error_log_mut(&mut self) -> Option<&mut PathBuf> {
        match self {
            ConfItem::Redirect(item) => item.error_log.as_mut(),
            ConfItem::Proxy(item) => item.error_log.as_mut(),
            ConfItem::CGI(item) => item.error_log.as_mut(),
            ConfItem::File(item) => item.error_log.as_mut(),
        }
    }
}


#[derive(Clone)]
pub struct CapsuleConf {
    pub listen: String,
    pub certificate: Option<Cert>,
    pub server_conf: Option<Arc<ServerConfig>>,
    pub pid_file: Option<String>,
    pub worker_processes: Option<u8>,
    pub items: Vec<ConfItem>,
    pub error_log: Option<PathBuf>,
    pub name: String,
}

impl Debug for CapsuleConf {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("CapsuleConf")
            .field("listen", &self.listen)
            .field("pid_file", &self.pid_file)
            .field("worker_processes", &self.worker_processes)
            .field("items", &self.items)
            .field("name", &self.name)
            .finish()
    }
}

#[derive(Debug, Clone)]
pub struct Substitution {
    pub regex: Regex,
    pub replace: String,
}

impl Substitution {
    pub fn new(s: impl AsRef<str>) -> Result<Self, GemcapsError> {
        let s = s.as_ref();
        let mut chars = s.chars();
        let first = chars.next().ok_or(
            GemcapsError::new("Expected a string")
        )?;
        let segments: Vec<&str> = chars.as_str().split(first).collect();
        if segments.len() > 2 || segments.len() <= 1 {
            return Err(GemcapsError::new("Expected a single separator e.g. `#foo#bar`"))
        }

        Ok(Self {
            regex: Regex::new(segments[0])?,
            replace: segments[1].to_string(),
        })
    }

    pub fn replace<'t>(&self, s: &'t str) -> Cow<'t, str> {
        self.regex.replace(s, &self.replace)
    }

    pub fn replace_all<'t>(&self, s: &'t str) -> Cow<'t, str> {
        self.regex.replace_all(s, &self.replace)
    }
}


#[derive(Debug, Clone, Deserialize, Default)]
pub struct ConfigurationItem {
    domain_names: Option<Vec<String>>,
    error_log: Option<String>,
    access_log: Option<String>,
    rule: Option<String>,
    substitution: Option<String>,

    redirect: Option<String>,
    proxy: Option<String>,
    cgi_root: Option<String>,
    file_root: Option<String>,
}

impl ConfigurationItem {
    fn populate_defaults(&mut self, defaults: &ConfigurationItem) {
        if let Some(error_log) = &defaults.error_log {
            if self.error_log == None {
                self.error_log = Some(error_log.to_owned());
            }
        }
        if let Some(access_log) = &defaults.access_log {
            if self.access_log == None {
                self.access_log = Some(access_log.to_owned());
            }
        }
        if let Some(rule) = &defaults.rule {
            if self.rule == None {
                self.rule = Some(rule.to_owned());
            }
        }
        if let Some(substitution) = &defaults.substitution {
            if self.substitution == None {
                self.substitution = Some(substitution.to_owned());
            }
        }
        if let Some(domain_names) = &defaults.domain_names {
            if self.domain_names == None {
                self.domain_names = Some(domain_names.to_owned());
            }
        }
    }

}

impl TryFrom<ConfigurationItem> for ConfItem {
    type Error = GemcapsError;

    fn try_from(value: ConfigurationItem) -> Result<Self, Self::Error> {
        Ok(if value.redirect != None {
            ConfItem::Redirect(RedirectConf::try_from(value)?)
        } else if value.proxy != None {
            ConfItem::Proxy(ProxyConf::try_from(value)?)
        } else if value.cgi_root != None {
            ConfItem::CGI(CGIConf::try_from(value)?)
        } else if value.file_root != None {
            ConfItem::File(FileConf::try_from(value)?)
        } else {
            unreachable!();
        })
    }
}

fn assert_none<T, S: AsRef<str>>(value: Option<T>, name: S) -> Result<(), GemcapsError> {
    match value {
        Some(_) => Err(GemcapsError::new(format!("Unexpected token `{}`", name.as_ref()))),
        None => Ok(())
    }
}

impl TryFrom<ConfigurationItem> for RedirectConf {
    type Error = GemcapsError;

    fn try_from(value: ConfigurationItem) -> Result<Self, Self::Error> {
        assert_none(value.cgi_root, "cgi_root")?;
        assert_none(value.file_root, "file_root")?;
        assert_none(value.proxy, "proxy")?;
        assert_none(value.rule, "rule")?;
        assert_none(value.substitution, "substitution")?;
        Ok(Self {
            domain_names: value.domain_names.ok_or(GemcapsError::new("Expected domain_names"))?
                .into_iter().map(|v| Glob::new(v)).collect(),
            redirect: value.redirect.ok_or(GemcapsError::new("Expected Redirect"))?,
            error_log: value.error_log.map(|v| PathBuf::from(v)),
            access_log: value.access_log.map(|v| PathBuf::from(v)),
        })
    }
}

impl TryFrom<ConfigurationItem> for ProxyConf {
    type Error = GemcapsError;

    fn try_from(value: ConfigurationItem) -> Result<Self, Self::Error> {
        assert_none(value.cgi_root, "cgi_root")?;
        assert_none(value.file_root, "file_root")?;
        assert_none(value.redirect, "redirect")?;
        Ok(Self {
            domain_names: value.domain_names.ok_or(GemcapsError::new("Expected domain_names"))?
                .into_iter().map(|v| Glob::new(v)).collect(),
            proxy: Request::new(value.proxy.ok_or(GemcapsError::new("Expected Redirect"))?)?,
            error_log: value.error_log.map(|v| PathBuf::from(v)),
            access_log: value.access_log.map(|v| PathBuf::from(v)),
            rule: match value.rule {
                Some(rule) => Some(Regex::new(&rule)?),
                None => None,
            },
            substitution: match value.substitution {
                Some(substitution) => Some(Substitution::new(substitution)?),
                None => None,
            } 
        })
    }
}

impl TryFrom<ConfigurationItem> for CGIConf {
    type Error = GemcapsError;

    fn try_from(value: ConfigurationItem) -> Result<Self, Self::Error> {
        assert_none(value.proxy, "proxy")?;
        assert_none(value.file_root, "file_root")?;
        assert_none(value.redirect, "redirect")?;
        Ok(Self {
            domain_names: value.domain_names.ok_or(GemcapsError::new("Expected domain_names"))?
                .into_iter().map(|v| Glob::new(v)).collect(),
            cgi_root: PathBuf::from(value.cgi_root.ok_or(GemcapsError::new("Expected Redirect"))?),
            error_log: value.error_log.map(|v| PathBuf::from(v)),
            access_log: value.access_log.map(|v| PathBuf::from(v)),
            rule: match value.rule {
                Some(rule) => Some(Regex::new(&rule)?),
                None => None,
            },
            substitution: match value.substitution {
                Some(substitution) => Some(Substitution::new(substitution)?),
                None => None,
            } 
        })
    }
}

impl TryFrom<ConfigurationItem> for FileConf {
    type Error = GemcapsError;

    fn try_from(value: ConfigurationItem) -> Result<Self, Self::Error> {
        assert_none(value.proxy, "proxy")?;
        assert_none(value.cgi_root, "cgi_root")?;
        assert_none(value.redirect, "redirect")?;
        Ok(Self {
            domain_names: value.domain_names.ok_or(GemcapsError::new("Expected domain_names"))?
                .into_iter().map(|v| Glob::new(v)).collect(),
            file_root: PathBuf::from(value.file_root.ok_or(GemcapsError::new("Expected Redirect"))?),
            error_log: value.error_log.map(|v| PathBuf::from(v)),
            access_log: value.access_log.map(|v| PathBuf::from(v)),
            rule: match value.rule {
                Some(rule) => Some(Regex::new(&rule)?),
                None => None,
            },
            substitution: match value.substitution {
                Some(substitution) => Some(Substitution::new(substitution)?),
                None => None,
            } 
        })
    }
}


#[derive(Debug, Clone, Deserialize, Default)]
pub struct Configuration {
    listen: Option<String>,
    certificate: Option<String>,
    certificate_key: Option<String>,
    pid_file: Option<String>,
    worker_processes: Option<u8>,

    error_log: Option<String>,
    access_log: Option<String>,
    rule: Option<String>,
    substitution: Option<String>,
    domain_names: Option<Vec<String>>,

    application: Vec<ConfigurationItem>,
}

impl TryFrom<Configuration> for CapsuleConf {
    type Error = GemcapsError;

    fn try_from(value: Configuration) -> Result<Self, Self::Error> {
        let mut items = Vec::new();

        let defaults = ConfigurationItem {
            domain_names: value.domain_names,
            error_log: value.error_log.clone(),
            access_log: value.access_log,
            rule: value.rule, 
            substitution: value.substitution,
            redirect: None, proxy: None, cgi_root: None, file_root: None ,
        };

        for mut item in value.application {
            item.populate_defaults(&defaults);
            items.push(item.try_into()?);
        }
        
        let certificate = if value.certificate != None && value.certificate_key != None {
            Some(Cert::new(
                value.certificate.unwrap(), 
                value.certificate_key.unwrap()
            ))
        } else if value.certificate == None && value.certificate_key == None {
            None
        } else {
            return Err(GemcapsError::new("certificate and certificate_key are needed together"));
        };

        Ok(Self {
            items,
            worker_processes: value.worker_processes,
            pid_file: value.pid_file,
            certificate,
            server_conf: None,
            listen: value.listen.or(Some("0.0.0.0:1965".into())).unwrap(),
            error_log: value.error_log.map(
                |val| PathBuf::from(val)
            ),
            name: String::default(),
        })
    }
}


#[cfg(test)]
mod tests {
    use super::*;
    use toml;
   
    #[test]
    fn test_redirect() {
        let config: Configuration = toml::from_str("
        [[application]]
        domain_names = [\"foobar.com\"]
        redirect = \"gemini://localhost:1970\"
        ").unwrap();

        let conf = CapsuleConf::try_from(config).unwrap();
        assert_eq!(conf.items.len(), 1);
        if let ConfItem::Redirect(_) = conf.items[0] {} else {
            panic!("Item should have been a redirect");
        }
    }

    #[test]
    fn test_invalid_redirect() {
        let config: Configuration = toml::from_str("
        [[application]]
        domain_names = [\"foobar.com\"]
        redirect = \"gemini://localhost:1970\"
        rule = \"/app\"
        ").unwrap();

        CapsuleConf::try_from(config).expect_err("rule should have been invalid");
    }

    #[test]
    fn test_proxy() {
        let config: Configuration = toml::from_str("
        [[application]]
        domain_names = [\"foobar.com\"]
        proxy = \"gemini://localhost:1970\"
        rule = \"/app\"
        substitution = \"#/app#\"
        ").unwrap();

        let conf = CapsuleConf::try_from(config).unwrap();
        assert_eq!(conf.items.len(), 1);
        if let ConfItem::Proxy(_) = conf.items[0] {} else {
            panic!("Item should have been a proxy");
        }
    }

    #[test]
    fn test_cgi() {
        let config: Configuration = toml::from_str("
        [[application]]
        domain_names = [\"foobar.com\"]
        cgi_root = \"gemini/foobar\"
        rule = \"\\\\.py$\"
        ").unwrap();

        let conf = CapsuleConf::try_from(config).unwrap();
        assert_eq!(conf.items.len(), 1);
        if let ConfItem::CGI(_) = conf.items[0] {} else {
            panic!("Item should have been a cgi");
        }
    }

    #[test]
    fn test_file() {
        let config: Configuration = toml::from_str("
        [[application]]
        domain_names = [\"foobar.com\"]
        file_root = \"gemini/foobar\"
        ").unwrap();

        let conf = CapsuleConf::try_from(config).unwrap();
        assert_eq!(conf.items.len(), 1);
        if let ConfItem::File(_) = conf.items[0] {} else {
            panic!("Item should have been a file");
        }
    }

    #[test]
    fn test_full_config() {
        let config: Configuration = toml::from_str("
            certificate = \"certs/foobar.crt\"
            certificate_key = \"certs/foobar.key\"
            domain_names = [\"foobar.com\"]
            error_log = \"logs/error.log\"
            access_log = \"logs/access.log\"

            [[application]]
            domain_names = [\"*.foobar.com\"]
            redirect = \"gemini://localhost:1970\"

            [[application]]
            rule = \"^/app/\"
            proxy = \"gemini://localhost:1971\"
        
            [[application]]
            cgi_root = \"gemini/foobar\"
            rule = \"^/cgi-bin/\"

            [[application]]
            file_root = \"gemini/foobar\"
        ").unwrap();

        let conf = CapsuleConf::try_from(config).unwrap();
        assert_eq!(conf.items.len(), 4);
        if let ConfItem::Redirect(redirect) = &conf.items[0] {
            assert_eq!(redirect.domain_names[0], Glob::new("*.foobar.com"));
            assert_eq!(redirect.error_log, Some(PathBuf::from("logs/error.log")));
            assert_eq!(redirect.access_log, Some(PathBuf::from("logs/access.log")));
        } else {
            panic!("First item should have been a redirect");
        }
        if let ConfItem::Proxy(proxy) = &conf.items[1] {
            assert_eq!(proxy.domain_names[0], Glob::new("foobar.com"));
            assert_eq!(proxy.error_log, Some(PathBuf::from("logs/error.log")));
            assert_eq!(proxy.access_log, Some(PathBuf::from("logs/access.log")));
        } else {
            panic!("Second item should have been a proxy");
        }
        if let ConfItem::CGI(_) = conf.items[2] {} else {
            panic!("Third item should have been a cgi");
        }
        if let ConfItem::File(_) = conf.items[3] {} else {
            panic!("Third item should have been a file");
        }


    }
}

