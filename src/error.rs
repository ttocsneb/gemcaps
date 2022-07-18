use std::{error::Error, fmt::Display, io, string::FromUtf8Error};


#[derive(Debug)]
pub enum GemcapsError {
    Message(String),
    Io(io::Error),
    Embedded(Box<dyn std::error::Error + Send + Sync>),
}

impl Error for GemcapsError {}

impl Display for GemcapsError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self {
            GemcapsError::Embedded(err) => err.fmt(f),
            GemcapsError::Io(err) => err.fmt(f),
            GemcapsError::Message(msg) => f.write_str(msg),
        }
    }
}

impl GemcapsError {
    #[inline]
    pub fn new(message: impl Into<String>) -> Self {
        Self::Message(message.into())
    }
}

impl From<Box<dyn std::error::Error + Send + Sync>> for GemcapsError {
    fn from(err: Box<dyn std::error::Error + Send + Sync>) -> Self {
        Self::Embedded(err)
    }
}

impl From<io::Error> for GemcapsError {
    fn from(err: io::Error) -> Self {
        Self::Io(err)
    }
}

impl From<regex::Error> for GemcapsError {
    fn from(err: regex::Error) -> Self {
        Self::Embedded(Box::new(err))
    }
}

impl From<FromUtf8Error> for GemcapsError {
    fn from(err: FromUtf8Error) -> Self {
        Self::Embedded(Box::new(err))
    }
}

impl From<rustls::Error> for GemcapsError {
    fn from(err: rustls::Error) -> Self {
        Self::Embedded(Box::new(err))
    }
}
