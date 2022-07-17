use std::{ops::Range, fmt::Display};

use regex::Regex;
use lazy_static::lazy_static;

use crate::error::GemcapsError;

#[derive(Debug, Clone, Hash, PartialEq, Eq, Default)]
pub struct Request {
    uri: String,
    protocol: Range<usize>,
    domain: Range<usize>,
    port: Range<usize>,
    path: Range<usize>,
    query: Range<usize>,
}

impl Request {
    pub fn new(uri: impl Into<String>) -> Result<Self, GemcapsError> {
        lazy_static! {
            // 1: protocol
            // 2: domain
            // 3: port
            // 4: path
            // 5: query
            static ref PARSER: Regex = Regex::new(
                r"([^:]+)://([^:/?\r\n]+)(:\d+|)([^?\r\n]*)(\?[^\r\n]*|)"
            ).unwrap();
        }
        let uri = uri.into();

        let captures = PARSER.captures(&uri).ok_or_else(
            || GemcapsError::new("Invalid request URI")
        )?;

        let protocol = captures.get(1).map_or(0..0, |c| c.range());
        let domain = captures.get(2).map_or(0..0, |c| c.range());
        let path = captures.get(4).map_or(0..0, |c| c.range());
    
        let mut port = captures.get(3).map_or(0..0, |c| c.range());
        port.next();
        let port = port;

        let mut query = captures.get(5).map_or(0..0, |c| c.range());
        query.next();
        let query = query;
        Ok(Self { uri, protocol, domain, path, port, query })
    }

    #[inline]
    pub fn protocol<'t>(&'t self) -> &'t str {
        &self.uri[self.protocol.to_owned()]
    }

    #[inline]
    pub fn domain<'t>(&'t self) -> &'t str {
        &self.uri[self.domain.to_owned()]
    }

    #[inline]
    pub fn port<'t>(&'t self) -> &'t str {
        &self.uri[self.port.to_owned()]
    }

    #[inline]
    pub fn path<'t>(&'t self) -> &'t str {
        &self.uri[self.path.to_owned()]
    }
    
    #[inline]
    pub fn query<'t>(&'t self) -> &'t str {
        &self.uri[self.query.to_owned()]
    }

    #[inline]
    pub fn uri<'t>(&'t self) -> &'t str {
        &self.uri[self.protocol.start..self.query.end]
    }
}

impl TryFrom<String> for Request {
    type Error = GemcapsError;

    fn try_from(value: String) -> Result<Self, Self::Error> {
        Self::new(value)
    }
}

impl Display for Request {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.uri())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_request() {
        let req = Request::new("gemini://localhost:1970/foo/?asdf\r\n").unwrap();
        assert_eq!(req.protocol(), "gemini");
        assert_eq!(req.domain(), "localhost");
        assert_eq!(req.port(), "1970");
        assert_eq!(req.path(), "/foo/");
        assert_eq!(req.query(), "asdf");
    }

    #[test]
    fn test_request_no_query() {
        let req = Request::new("gemini://localhost/\r\n").unwrap();
        assert_eq!(req.protocol(), "gemini");
        assert_eq!(req.domain(), "localhost");
        assert_eq!(req.port(), "");
        assert_eq!(req.path(), "/");
        assert_eq!(req.query(), "");
    }

    #[test]
    fn test_request_no_path() {
        let req = Request::new("gemini://localhost?asdf\r\n").unwrap();
        assert_eq!(req.protocol(), "gemini");
        assert_eq!(req.domain(), "localhost");
        assert_eq!(req.port(), "");
        assert_eq!(req.path(), "");
        assert_eq!(req.query(), "asdf");
    }

    #[test]
    fn test_request_domain_only() {
        let req = Request::new("gemini://localhost\r\n").unwrap();
        assert_eq!(req.protocol(), "gemini");
        assert_eq!(req.domain(), "localhost");
        assert_eq!(req.port(), "");
        assert_eq!(req.path(), "");
        assert_eq!(req.query(), "");
    }

    #[test]
    fn test_request_bad() {
        Request::new("gemini:/localhost\r\n").expect_err("Should have an error");
    }

}