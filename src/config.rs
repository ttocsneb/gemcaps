use std::{path::PathBuf, fmt::Debug, sync::Arc};

use regex::Regex;
use rustls::ServerConfig;
use serde::Deserialize;

pub mod redirect;
pub mod proxy;
pub mod cgi;
pub mod file;

use crate::{error::GemcapsError, pem::Cert, glob::Glob};

use self::cgi::CgiConf;
use self::proxy::ProxyConf;
use self::redirect::RedirectConf;
use self::file::FileConf;

pub trait ConfItemTrait {
    fn domain_names(&self) -> &Vec<Glob>;

    fn rule(&self) -> Option<&Regex>;

    fn error_log(&self) -> Option<&PathBuf>;
    fn error_log_mut(&mut self) -> Option<&mut PathBuf>;

    fn access_log(&self) -> Option<&PathBuf>;
    fn access_log_mut(&mut self) -> Option<&mut PathBuf>;

    fn apply_default_domain_names(&mut self, domain_names: &Vec<Glob>);
    fn apply_default_rule(&mut self, rule: &RegexDe);
    fn apply_default_error_log(&mut self, error_log: &PathBuf);
    fn apply_default_access_log(&mut self, access_log: &PathBuf);
}


#[derive(Debug, Clone, Deserialize)]
#[serde(untagged)]
pub enum ConfItem {
    Redirect(RedirectConf),
    Proxy(ProxyConf),
    CGI(CgiConf),
    File(FileConf),
}

impl ConfItem {
    #[inline]
    fn conf_item(&self) -> &dyn ConfItemTrait {
        match self {
            ConfItem::Redirect(redirect) => redirect as &dyn ConfItemTrait,
            ConfItem::Proxy(proxy) => proxy as &dyn ConfItemTrait,
            ConfItem::CGI(cgi) => cgi as &dyn ConfItemTrait,
            ConfItem::File(file) => file as &dyn ConfItemTrait,
        }
    }

    #[inline]
    fn conf_item_mut(&mut self) -> &mut dyn ConfItemTrait {
        match self {
            ConfItem::Redirect(redirect) => redirect as &mut dyn ConfItemTrait,
            ConfItem::Proxy(proxy) => proxy as &mut dyn ConfItemTrait,
            ConfItem::CGI(cgi) => cgi as &mut dyn ConfItemTrait,
            ConfItem::File(file) => file as &mut dyn ConfItemTrait,
        }
    }

}

impl ConfItemTrait for ConfItem {
    fn domain_names(&self) -> &Vec<Glob> {
        self.conf_item().domain_names()
    }

    fn rule(&self) -> Option<&Regex> {
        self.conf_item().rule()
    }

    fn error_log(&self) -> Option<&PathBuf> {
        self.conf_item().error_log()
    }
    fn error_log_mut(&mut self) -> Option<&mut PathBuf> {
        self.conf_item_mut().error_log_mut()
    }

    fn access_log(&self) -> Option<&PathBuf> {
        self.conf_item().access_log()
    }
    fn access_log_mut(&mut self) -> Option<&mut PathBuf> {
        self.conf_item_mut().access_log_mut()
    }

    fn apply_default_domain_names(&mut self, domain_names: &Vec<Glob>) {
        self.conf_item_mut().apply_default_domain_names(domain_names)
    }
    fn apply_default_rule(&mut self, rule: &RegexDe) {
        self.conf_item_mut().apply_default_rule(rule)
    }
    fn apply_default_error_log(&mut self, error_log: &PathBuf) {
        self.conf_item_mut().apply_default_error_log(error_log)
    }
    fn apply_default_access_log(&mut self, access_log: &PathBuf) {
        self.conf_item_mut().apply_default_access_log(access_log)
    }
}

#[derive(Clone)]
pub struct CapsuleConf {
    pub listen: String,
    pub certificate: Option<Cert>,
    pub server_conf: Option<Arc<ServerConfig>>,
    pub items: Vec<ConfItem>,
    pub error_log: Option<PathBuf>,
    pub name: String,
}

impl Debug for CapsuleConf {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("CapsuleConf")
            .field("listen", &self.listen)
            .field("items", &self.items)
            .field("name", &self.name)
            .finish()
    }
}

#[derive(Debug, Clone, Deserialize, Default)]
pub struct ConfDeserializer {
    listen: Option<String>,
    certificate: Option<PathBuf>,
    certificate_key: Option<PathBuf>,

    error_log: Option<PathBuf>,
    access_log: Option<PathBuf>,
    rule: Option<RegexDe>,
    domain_names: Option<Vec<Glob>>,

    application: Vec<ConfItem>,
}

impl TryFrom<ConfDeserializer> for CapsuleConf {
    type Error = GemcapsError;

    fn try_from(mut value: ConfDeserializer) -> Result<Self, Self::Error> {
        for item in &mut value.application {
            if let Some(error_log) = &value.error_log {
                item.apply_default_error_log(error_log);
            }
            if let Some(access_log) = &value.access_log {
                item.apply_default_access_log(access_log);
            }
            if let Some(rule) = &value.rule {
                item.apply_default_rule(rule);
            }
            if let Some(domain_names) = &value.domain_names {
                item.apply_default_domain_names(domain_names);
            }
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
            items: value.application,
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


#[derive(Debug, Clone, Deserialize)]
pub struct RegexDe(
    #[serde(with = "serde_regex")]
    Regex
);

impl RegexDe {
    #[inline]
    pub fn is_match(&self, text: &str) -> bool {
        self.0.is_match(text)
    }
}

impl AsRef<Regex> for RegexDe {
    fn as_ref(&self) -> &Regex {
        &self.0
    }
}

impl AsMut<Regex> for RegexDe {
    fn as_mut(&mut self) -> &mut Regex {
        &mut self.0
    }
}

impl From<RegexDe> for Regex {
    fn from(value: RegexDe) -> Self {
        value.0
    }
}


#[cfg(test)]
mod tests {
    use super::*;
    use toml;
   
    #[test]
    fn test_redirect() {
        let config: ConfDeserializer = toml::from_str("
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
    fn test_proxy() {
        let config: ConfDeserializer = toml::from_str("
        [[application]]
        domain_names = [\"foobar.com\"]
        proxy = \"gemini://localhost:1970\"
        rule = \"/app\"
        ").unwrap();

        let conf = CapsuleConf::try_from(config).unwrap();
        assert_eq!(conf.items.len(), 1);
        if let ConfItem::Proxy(_) = conf.items[0] {} else {
            panic!("Item should have been a proxy");
        }
    }

    #[test]
    fn test_cgi() {
        let config: ConfDeserializer = toml::from_str("
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
        let config: ConfDeserializer = toml::from_str("
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
        let config: ConfDeserializer = toml::from_str("
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

