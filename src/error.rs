use std::{error::Error, fmt::Display, io, string::FromUtf8Error};


#[derive(Debug)]
pub enum GemcapsError {
    Message(String),
    Embedded(Box<dyn std::error::Error + Send + Sync>),
}

impl Error for GemcapsError {}

impl Display for GemcapsError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self {
            GemcapsError::Embedded(err) => err.fmt(f),
            GemcapsError::Message(msg) => f.write_str(msg),
        }
    }
}

impl GemcapsError {
    #[inline]
    pub fn new(message: impl Into<String>) -> Self {
        Self::Message(message.into())
    }

    #[inline]
    pub fn err(err: impl std::error::Error + Send + Sync + 'static) -> Self {
        Self::Embedded(Box::new(err))
    }

    #[inline]
    pub fn err_box(err: Box<dyn std::error::Error + Send + Sync>) -> Self {
        Self::Embedded(err)
    }
}

impl From<Box<dyn std::error::Error + Send + Sync>> for GemcapsError {
    fn from(err: Box<dyn std::error::Error + Send + Sync>) -> Self {
        Self::err_box(err)
    }
}

impl From<io::Error> for GemcapsError {
    fn from(err: io::Error) -> Self {
        Self::err(err)
    }
}

impl From<regex::Error> for GemcapsError {
    fn from(err: regex::Error) -> Self {
        Self::err(err)
    }
}

impl From<FromUtf8Error> for GemcapsError {
    fn from(err: FromUtf8Error) -> Self {
        Self::err(err)
    }
}

impl From<rustls::Error> for GemcapsError {
    fn from(err: rustls::Error) -> Self {
        Self::err(err)
    }
}
