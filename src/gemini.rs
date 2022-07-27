use std::{ops::Range, fmt::Display, ffi::{OsString, OsStr}, os::unix::prelude::OsStrExt};

use regex::{Regex, bytes};
use lazy_static::lazy_static;
use serde::{Deserialize, de::Visitor};

use crate::error::{GemcapsError, GemcapsResult};

#[derive(Debug, Clone, Hash, PartialEq, Eq, Default)]
pub struct Request {
    uri: String,
    protocol: Range<usize>,
    domain: Range<usize>,
    port: Range<usize>,
    path: Range<usize>,
    query: Range<usize>,
}

#[allow(dead_code)]
impl Request {
    /// Parse the segments of a uri
    pub fn new(uri: impl Into<String>) -> GemcapsResult<Self> {
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

struct RequestVisitor;
impl<'de> Visitor<'de> for RequestVisitor {
    type Value = Request;

    fn visit_string<E>(self, v: String) -> Result<Self::Value, E>
        where
            E: serde::de::Error, {
        Request::new(v).map_err(
            |err| E::custom(err.to_string())
        )
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
        where
            E: serde::de::Error, {
        Request::new(v).map_err(
            |err| E::custom(err.to_string())
        )
    }

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("a request string")
    }
}

impl<'de> Deserialize<'de> for Request {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: serde::Deserializer<'de> {
        deserializer.deserialize_string(RequestVisitor)
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

/// A Response wrapper to make creating responses easier.
/// 
/// # Example
/// 
/// ```
/// let response = Response::redirect_perm("/foo/bar");
/// assert_eq!(response.to_string(), "31 /foo/bar\r\n");
/// ```
#[allow(dead_code)]
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Response {
    /// 10 INPUT
    Input { prompt: String },
    /// 11 SENSITIVE INPUT
    InputSensitive { prompt: String },
    /// 20 SUCCESS
    Success {
        meta: String,
        body: OsString,
    },
    /// 30 TEMPORARY REDIRECT
    RedirectTemp { redirect: String },
    /// 31 PERMANENT REDIRECT
    RedirectPerm { redirect: String },
    /// 40 TEMPORARY FAILURE
    FailureTemp { message: String },
    /// 41 SERVER UNAVAILABLE
    ServerUnavail { message: String },
    /// 42 CGI ERROR
    CgiError { message: String },
    /// 43 PROXY ERROR
    ProxyError { message: String },
    /// 44 SLOW DOWN
    SlowDown { message: String },
    /// 50 PERMANENT FAILURE
    FailurePerm { message: String },
    /// 51 NOT FOUND
    NotFound { message: String },
    /// 52 GONE
    Gone { message: String },
    /// 53 PROXY REFUSED
    ProxyRefused { message: String },
    /// 59 BAD REQUEST
    BadRequest { message: String },
    /// 60 CERTIFICATE REQUIRED
    CertRequired { message: String },
    /// 61 CERTIFICATE NOT AUTHORIZED
    CertNotAuthorized { message: String },
    /// 62 CERTIFICATE NOT VALID
    CertInvalid { message: String },
}

#[allow(dead_code)]
impl Response {
    /// 10 INPUT
    /// 
    /// Status codes beginning with 1 are INPUT status codes, meaning:
    /// 
    /// The requested resource accepts a line of textual user input. The <META>
    /// line is a prompt which should be displayed to the user. The same 
    /// resource should then be requested again with the user's input included
    /// as a query component. Queries are included in requests as per the usual
    /// generic URL definition in RFC3986, i.e. separated from the path by a ?.
    /// Reserved characters used in the user's input must be "percent-encoded"
    /// as per RFC3986, and space characters should also be percent-encoded.
    pub fn input(prompt: impl Into<String>) -> Self {
        Self::Input { prompt: prompt.into() }
    }

    /// 11 SENSITIVE INPUT
    /// 
    /// As per status code 10, but for use with sensitive input such as
    /// passwords. Clients should present the prompt as per status code 10, but
    /// the user's input should not be echoed to the screen to prevent it being
    /// read by "shoulder surfers".
    pub fn input_sensitive(prompt: impl Into<String>) -> Self {
        Self::InputSensitive { prompt: prompt.into() }
    }
   
    /// 20 SUCCESS
    /// 
    /// Status codes beginning with 2 are SUCCESS status codes, meaning:
    /// 
    /// The request was handled successfully and a response body will follow
    /// the response header. The <META> line is a MIME media type which applies
    /// to the response body.
    pub fn success(meta: impl Into<String>, body: impl Into<OsString>) -> Self {
        Self::Success { meta: meta.into(), body: body.into() }
    }
   
    /// 30 TEMPORARY REDIRECT
    /// 
    /// Status codes beginning with 3 are REDIRECT status codes, meaning:
    ///
    /// The server is redirecting the client to a new location for the
    /// requested resource. There is no response body. <META> is a new URL for
    /// the requested resource. The URL may be absolute or relative. If
    /// relative, it should be resolved against the URL used in the original
    /// request. If the URL used in the original request contained a query
    /// string, the client MUST NOT apply this string to the redirect URL,
    /// instead using the redirect URL "as is". The redirect should be
    /// considered temporary, i.e. clients should continue to request the
    /// resource at the original address and should not perform convenience
    /// actions like automatically updating bookmarks. There is no response
    /// body.
    pub fn redirect_temp(redirect: impl Into<String>) -> Self {
        Self::RedirectTemp { redirect: redirect.into() }
    }
    
    /// 31 PERMANENT REDIRECT
    /// 
    /// The requested resource should be consistently requested from the new
    /// URL provided in future. Tools like search engine indexers or content
    /// aggregators should update their configurations to avoid requesting the
    /// old URL, and end-user clients may automatically update bookmarks, etc.
    /// Note that clients which only pay attention to the initial digit of
    /// status codes will treat this as a temporary redirect. They will still
    /// end up at the right place, they just won't be able to make use of the
    /// knowledge that this redirect is permanent, so they'll pay a small
    /// performance penalty by having to follow the redirect each time.
    pub fn redirect_perm(redirect: impl Into<String>) -> Self {
        Self::RedirectPerm { redirect: redirect.into() }
    }
    /// 40 TEMPORARY FAILURE
    /// 
    /// Status codes beginning with 4 are TEMPORARY FAILURE status codes,
    /// meaning:
    ///
    /// The request has failed. There is no response body. The nature of the
    /// failure is temporary, i.e. an identical request MAY succeed in the
    /// future. The contents of <META> may provide additional information on
    /// the failure, and should be displayed to human users.
    pub fn failure_temp(message: impl Into<String>) -> Self {
        Self::FailureTemp { message: message.into() }
    }

    /// 41 SERVER UNAVAILABLE
    /// 
    /// The server is unavailable due to overload or maintenance. (cf HTTP 503)
    pub fn server_unavail(message: impl Into<String>) -> Self {
        Self::ServerUnavail { message: message.into() }
    }

    /// 42 CGI ERROR
    /// 
    /// A CGI process, or similar system for generating dynamic content, died
    /// unexpectedly or timed out.
    pub fn cgi_error(message: impl Into<String>) -> Self {
        Self::CgiError { message: message.into() }
    }

    /// 43 PROXY ERROR
    /// 
    /// A proxy request failed because the server was unable to successfully
    /// complete a transaction with the remote host. (cf HTTP 502, 504)
    pub fn proxy_error(message: impl Into<String>) -> Self {
        Self::ProxyError { message: message.into() }
    }

    /// 44 SLOW DOWN
    /// 
    /// Rate limiting is in effect. <META> is an integer number of seconds
    /// which the client must wait before another request is made to this
    /// server. (cf HTTP 429)
    pub fn slow_down(message: impl Into<String>) -> Self {
        Self::SlowDown { message: message.into() }
    }

    /// 50 PERMANENT FAILURE
    /// 
    /// Status codes beginning with 5 are PERMANENT FAILURE status codes,
    /// meaning:
    /// 
    /// The request has failed. There is no response body. The nature of the
    /// failure is permanent, i.e. identical future requests will reliably fail
    /// for the same reason. The contents of <META> may provide additional
    /// information on the failure, and should be displayed to human users.
    /// Automatic clients such as aggregators or indexing crawlers should not
    /// repeat this request.
    pub fn failure_perm(message: impl Into<String>) -> Self {
        Self::FailurePerm { message: message.into() }
    }

    /// 51 NOT FOUND
    /// 
    /// The requested resource could not be found but may be available in the
    /// future. (cf HTTP 404) (struggling to remember this important status
    /// code? Easy: you can't find things hidden at Area 51!)
    pub fn not_found(message: impl Into<String>) -> Self {
        Self::NotFound { message: message.into() }
    }

    /// 52 GONE
    /// 
    /// The resource requested is no longer available and will not be available
    /// again. Search engines and similar tools should remove this resource
    /// from their indices. Content aggregators should stop requesting the
    /// resource and convey to their human users that the subscribed resource
    /// is gone. (cf HTTP 410)
    pub fn gone(message: impl Into<String>) -> Self {
        Self::Gone { message: message.into() }
    }

    /// 53 PROXY REFUSED
    /// 
    /// The request was for a resource at a domain not served by the server and
    /// the server does not accept proxy requests.
    pub fn proxy_refused(message: impl Into<String>) -> Self {
        Self::ProxyRefused { message: message.into() }
    }

    /// 59 BAD REQUEST
    /// 
    /// The server was unable to parse the client's request, presumably due to
    /// a malformed request. (cf HTTP 400)
    pub fn bad_request(message: impl Into<String>) -> Self {
        Self::BadRequest { message: message.into() }
    }

    /// 60 CLIENT CERTIFICATE REQUIRED
    /// 
    /// Status codes beginning with 6 are CLIENT CERTIFICATE REQUIRED status
    /// codes, meaning:
    ///
    /// The requested resource requires a client certificate to access. If the
    /// request was made without a certificate, it should be repeated with one.
    /// If the request was made with a certificate, the server did not accept
    /// it and the request should be repeated with a different certificate. The
    /// contents of <META> (and/or the specific 6x code) may provide additional
    /// information on certificate requirements or the reason a certificate was
    /// rejected.
    pub fn cert_required(message: impl Into<String>) -> Self {
        Self::CertRequired { message: message.into() }
    }

    /// 61 CERTIFICATE NOT AUTHORIZED
    /// 
    /// The supplied client certificate is not authorised for accessing the
    /// particular requested resource. The problem is not with the certificate
    /// itself, which may be authorised for other resources.
    pub fn cert_not_authorized(message: impl Into<String>) -> Self {
        Self::CertNotAuthorized { message: message.into() }
    }

    /// 62 CERTIFICATE NOT VALID
    /// 
    /// The supplied client certificate was not accepted because it is not
    /// valid. This indicates a problem with the certificate in and of itself,
    /// with no consideration of the particular requested resource. The most
    /// likely cause is that the certificate's validity start date is in the
    /// future or its expiry date has passed, but this code may also indicate
    /// an invalid signature, or a violation of X509 standard requirements. The
    /// <META> should provide more information about the exact error.
    pub fn cert_invalid(message: impl Into<String>) -> Self {
        Self::CertInvalid { message: message.into() }
    }

    pub fn parse(body: impl AsRef<[u8]>) -> GemcapsResult<Self> {
        lazy_static! {
            static ref HEADER: bytes::Regex = bytes::Regex::new(r"^([\d]{2})\s*([^\r\n]*).*\n").unwrap();
        }
        let body = body.as_ref();
        let captures = HEADER.captures(body).ok_or(GemcapsError::new("Invalid response format"))?;

        let status = String::from_utf8(captures.get(1).unwrap().as_bytes().to_owned()).unwrap();
        let meta = String::from_utf8(captures.get(2).map(|m| m.as_bytes()).or(Some(b"")).unwrap().to_owned())?;
        let status = u8::from_str_radix(&status, 10).unwrap();

        Ok(match status {
            10 => Self::input(meta),
            11 => Self::input_sensitive(meta),
            20 => Self::success(meta, OsStr::from_bytes(&body[captures.get(0).unwrap().end()..])),
            30 => Self::redirect_temp(meta),
            31 => Self::redirect_perm(meta),
            40 => Self::failure_temp(meta),
            41 => Self::server_unavail(meta),
            42 => Self::cgi_error(meta),
            43 => Self::proxy_error(meta),
            44 => Self::slow_down(meta),
            50 => Self::failure_perm(meta),
            51 => Self::not_found(meta),
            52 => Self::gone(meta),
            53 => Self::proxy_refused(meta),
            59 => Self::bad_request(meta),
            60 => Self::cert_required(meta),
            61 => Self::cert_not_authorized(meta),
            62 => Self::cert_invalid(meta),
            _ => {
                return Err(GemcapsError::new("Invalid status"));
            }
        })
    }

    /// Get the <META> part of the response header
    pub fn meta(&self) -> &str {
        match self {
            Self::Input { prompt } => &prompt,
            Self::InputSensitive { prompt } => &prompt,
            Self::Success { meta, body: _ } => &meta,
            Self::RedirectTemp { redirect } => &redirect,
            Self::RedirectPerm { redirect } => &redirect,
            Self::FailureTemp { message } => &message,
            Self::ServerUnavail { message } => &message,
            Self::CgiError { message } => &message,
            Self::ProxyError { message } => &message,
            Self::SlowDown { message } => &message,
            Self::FailurePerm { message } => &message,
            Self::NotFound { message } => &message,
            Self::Gone { message } => &message,
            Self::ProxyRefused { message } => &message,
            Self::BadRequest { message } => &message,
            Self::CertRequired { message } => &message,
            Self::CertNotAuthorized { message } => &message,
            Self::CertInvalid { message } => &message,
        }
    }

    /// Get the <STATUS> part of the response header
    pub fn status(&self) -> u8 {
        match self {
            Self::Input { prompt: _ } => 10,
            Self::InputSensitive { prompt: _ } => 11,
            Self::Success { meta: _, body: _ } => 20,
            Self::RedirectTemp { redirect: _ } => 30,
            Self::RedirectPerm { redirect: _ } => 31,
            Self::FailureTemp { message: _ } => 40,
            Self::ServerUnavail { message: _ } => 41,
            Self::CgiError { message: _ } => 42,
            Self::ProxyError { message: _ } => 43,
            Self::SlowDown { message: _ } => 44,
            Self::FailurePerm { message: _ } => 50,
            Self::NotFound { message: _ } => 51,
            Self::Gone { message: _ } => 52,
            Self::ProxyRefused { message: _ } => 53,
            Self::BadRequest { message: _ } => 59,
            Self::CertRequired { message: _ } => 60,
            Self::CertNotAuthorized { message: _ } => 61,
            Self::CertInvalid { message: _ } => 62,
        }
    }

    /// Get the human readable name of this response type
    pub fn name(&self) -> &'static str {
        match self {
            Self::Input { prompt: _ } => "input",
            Self::InputSensitive { prompt: _ } => "sensitive input",
            Self::Success { meta: _, body: _ } => "success",
            Self::RedirectTemp { redirect: _ } => "temporary redirect",
            Self::RedirectPerm { redirect: _ } => "permanent redirect",
            Self::FailureTemp { message: _ } => "temporary failure",
            Self::ServerUnavail { message: _ } => "server unavailable",
            Self::CgiError { message: _ } => "cgi error",
            Self::ProxyError { message: _ } => "proxy error",
            Self::SlowDown { message: _ } => "slow down",
            Self::FailurePerm { message: _ } => "permanent failure",
            Self::NotFound { message: _ } => "not found",
            Self::Gone { message: _ } => "gone",
            Self::ProxyRefused { message: _ } => "proxy refused",
            Self::BadRequest { message: _ } => "bad request",
            Self::CertRequired { message: _ } => "certificate required",
            Self::CertNotAuthorized { message: _ } => "certificate not authorized",
            Self::CertInvalid { message: _ } => "certificate not valid",
        }
    }

    /// Get the full header of this response
    pub fn header(&self) -> String {
        format!("{} {}\r\n", self.status(), self.meta())
    }

    /// Get the body of this response
    pub fn body(&self) -> Option<&OsStr> {
        match &self {
            Self::Success { meta: _, body } => Some(body.as_ref()),
            _ => None
        }
    }

    /// Get safe human readable information about this response
    /// 
    /// This is a helper function that formats the status/name into a string
    pub fn info(&self) -> String {
        format!("{} ({})", self.status(), self.name())
    }
}

impl Display for Response {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.header())?;
        if let Some(body) = self.body() {
            f.write_str(&String::from_utf8_lossy(body.as_bytes()))?;
        };
        Ok(())
    }
}
