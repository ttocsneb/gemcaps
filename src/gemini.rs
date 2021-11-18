use std::{fmt::Debug, io};

fn parse_query(request: &str) -> String {
    let query = String::from(request);
    match query.find("\r\n") {
        Some(n) => {
            String::from(&query[0 .. n])
        },
        None => {
            match query.find("\n") {
                Some(n) => {
                    String::from(&query[0 .. n])
                }
                None => {
                    query
                }
            }
        },
    }
}

fn parse_path(request: &str) -> (String, String) {
    let path = String::from(request);
    match path.find("?") {
        Some(n) => {
            (String::from(&path[0 .. n]), parse_query(&path[n ..]))
        }
        None => {
            (parse_query(request), String::from(""))
        }
    }
}

fn parse_domain(request: &str) -> (String, String, String) {
    let domain = String::from(request);
    match domain.find("/") {
        Some(n) => {
            let (path, query) = parse_path(&domain[n ..]);
            (String::from(&domain[0 .. n]), path, query)
        }
        None => {
            match domain.find("?") {
                Some(n) => {
                    let query = parse_query(&domain[n ..]);
                    (String::from(&domain[0 .. n]), String::from(""), query)
                },
                None => {
                    (parse_query(&domain), String::from(""), String::from(""))
                }
            }
        }
    }
}

fn parse_request(request: &str) -> io::Result<(String, String, String, String)> {
    let protocol = String::from(request);
    match request.find("://") {
        Some(n) => {
            let (domain, path, query) = parse_domain(&protocol[n + 3 ..]);
            Ok((String::from(&protocol[0 .. n]), domain, path, query))
        },
        None => {
            Err(io::Error::new(io::ErrorKind::InvalidData, ""))
        }
    }
}


#[derive(Clone, Hash, PartialEq, Eq)]
pub struct Request {
    pub protocol: String,
    pub domain: String,
    pub path: String,
    pub query: String,
}


impl Request {
    pub fn parse(request: &str) -> io::Result<Self> {
        let (protocol, domain, path, query) = parse_request(request)?;
        Ok(Request { 
            protocol,
            domain, 
            path, 
            query,
        })
    }
}

impl Debug for Request {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_fmt(format_args!("{}://{}{}{}", self.protocol, self.domain, self.path, self.query))?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::gemini;

    #[test]
    fn test_request() {
        let req = gemini::Request::parse("gemini://localhost/?asdf\r\n").unwrap();
        assert_eq!(req.protocol, "gemini");
        assert_eq!(req.domain, "localhost");
        assert_eq!(req.path, "/");
        assert_eq!(req.query, "?asdf");
    }

    #[test]
    fn test_request_no_query() {
        let req = gemini::Request::parse("gemini://localhost/\r\n").unwrap();
        assert_eq!(req.protocol, "gemini");
        assert_eq!(req.domain, "localhost");
        assert_eq!(req.path, "/");
        assert_eq!(req.query, "");
    }

    #[test]
    fn test_request_no_path() {
        let req = gemini::Request::parse("gemini://localhost?asdf\r\n").unwrap();
        assert_eq!(req.protocol, "gemini");
        assert_eq!(req.domain, "localhost");
        assert_eq!(req.path, "");
        assert_eq!(req.query, "?asdf");
    }

    #[test]
    fn test_request_domain_only() {
        let req = gemini::Request::parse("gemini://localhost\r\n").unwrap();
        assert_eq!(req.protocol, "gemini");
        assert_eq!(req.domain, "localhost");
        assert_eq!(req.path, "");
        assert_eq!(req.query, "");
    }

    #[test]
    fn test_request_bad() {
        gemini::Request::parse("gemini:/localhost\r\n").expect_err("Should have an error");
    }

}