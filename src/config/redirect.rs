use std::path::PathBuf;

use regex::Regex;
use serde::Deserialize;

use crate::glob::Glob;

use super::{ConfItemTrait, RegexDe};

#[derive(Debug, Clone, Deserialize)]
pub struct RedirectConf {
    #[serde(default)]
    pub domain_names: Vec<Glob>,
    pub redirect: String,
    pub error_log: Option<PathBuf>,
    pub access_log: Option<PathBuf>,
}

impl ConfItemTrait for RedirectConf {
    #[inline]
    fn domain_names(&self) -> &Vec<Glob> {
        &self.domain_names
    }

    #[inline]
    fn rule(&self) -> Option<&Regex> {
        None
    }

    #[inline]
    fn error_log(&self) -> Option<&PathBuf> {
        self.error_log.as_ref()
    }

    #[inline]
    fn error_log_mut(&mut self) -> Option<&mut PathBuf> {
        self.error_log.as_mut()
    }

    #[inline]
    fn access_log(&self) -> Option<&PathBuf> {
        self.access_log.as_ref()
    }

    #[inline]
    fn access_log_mut(&mut self) -> Option<&mut PathBuf> {
        self.access_log.as_mut()
    }

    fn apply_default_domain_names(&mut self, domain_names: &Vec<Glob>) {
        if self.domain_names.is_empty() {
            self.domain_names = domain_names.clone()
        }
    }

    fn apply_default_rule(&mut self, _rule: &RegexDe) {}

    fn apply_default_error_log(&mut self, error_log: &PathBuf) {
        if let None = self.error_log {
            self.error_log = Some(error_log.clone())
        }
    }

    fn apply_default_access_log(&mut self, access_log: &PathBuf) {
        if let None = self.access_log {
            self.access_log = Some(access_log.clone())
        }
    }
}

