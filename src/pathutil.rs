use std::{collections::HashMap, io};

use regex::Regex;
use lazy_static::lazy_static;

/// Join two paths together.
/// 
/// This will make sure that there are no duplicate '/' where the two paths are
/// joined.
/// 
/// # Example
/// 
/// ```rust
/// let joined = join("asdf", "qwerty");
/// assert_eq!(joined, "asdf/qwerty");
/// ```
/// 
pub fn join(path_a: &str, path_b: &str) -> String {
    if path_a.ends_with("/") {
        return join(&path_a[0 .. path_a.len() - 1], path_b);
    }
    if path_b.starts_with("/") {
        return join(path_a, &path_b[1 ..]);
    }
    format!("{}/{}", path_a, path_b)
}

/// A variadic version of join
/// 
/// This simply expands to `join(a, &join(b, c))`
/// 
/// # Example
/// 
/// ```rust
/// let joined = join!("a", "b", "c");
/// assert_eq!(joined, "a/b/c");
/// ```
/// 
#[macro_export]
macro_rules! join {
    ($x:expr) => (
        $x
    );
    ($x:expr, $y:expr) => (
        join($x, $y)
    );
    ($x:expr $( , $more:expr)* ) => (
        join($x, &join!( $( $more ),* ))
    );
}

/// Make a path absolute given a base path
/// 
/// If path is an absolute path, then path is returned, otherwise
/// join(base, path) is returned
/// 
/// # Example
/// 
/// ```rust
/// let abs = abspath("/foo/bar", "/cheese/qwerty");
/// assert_eq!(abs, "/cheese/qwerty");
/// let abs = abspath("/foo/bar", "cheese/qwerty");
/// assert_eq!(abs, "/foo/bar/cheese/qwerty");
/// ```
/// 
pub fn abspath(base: &str, path: &str) -> String {
    if path.starts_with("/") {
        return String::from(path);
    }
    join(base, path)
}


/// Split the name from the extension
/// 
/// # Example
/// 
/// ```rust
/// let (name, ext) = splitext("foo.bar");
/// assert_eq!(name, "foo");
/// assert_eq!(ext, ".bar");
/// ```
pub fn splitext(path: &str) -> (String, String) {
    lazy_static! {
        static ref RE: Regex = Regex::new(r"(.*)(\..*)").unwrap();
    }
    let groups = match RE.captures(path) {
        Some(groups) => groups,
        None => {
            return (String::from(path), String::from(""));
        }
    };
    let name = String::from(groups.get(1).map_or("", |m| m.as_str()));
    let ext = String::from(groups.get(2).map_or("", |m| m.as_str()));
    (name, ext)
}

/// Get the final element in the path
/// 
/// # Example
/// 
/// ```rust
/// let name = basename("/foo/bar/cheese");
/// assert_eq!(name, "cheese");
/// ```
/// 
pub fn basename(path: &str) -> String {
    lazy_static! {
        static ref RE: Regex = Regex::new(r"/([^/]+)$").unwrap();
    }
    if path.ends_with("/") {
        return basename(&path[0 .. path.len() - 1]);
    }
    match RE.captures(path) {
        Some(m) => String::from(m.get(1).map_or("", |m| m.as_str())),
        None => String::from(path),
    }
}

/// Get the parent elements in the path
/// 
/// # Example
/// 
/// ```rust
/// let name = basename("/foo/bar/cheese");
/// assert_eq!(name, "/foo/bar/");
/// ```
/// 
pub fn dirname(path: &str) -> String {
    lazy_static! {
        static ref RE: Regex = Regex::new(r"^.*/").unwrap();
    }
    if path.ends_with("/") {
        return dirname(&path[0 .. path.len() - 1]);
    }
    match RE.find(path) {
        Some(m) => String::from(m.as_str()),
        None => String::from(""),
    }
}

/// Expand any '.' or '..' in a path
/// 
/// If the path is relative, then excess '..' will bleed over e.g.
/// 'foo/../../bar' -> '../bar'. However, if the path is absolute, an error is
/// returned when the path tries to beyond the root.
/// 
/// # Example
/// 
/// ```rust
/// let expanded = expand("foo/bar/../../../cheese/../yeet/").unwrap();
/// assert_eq!(expanded, "../yeet/");
/// ```
/// 
pub fn expand(path: &str) -> io::Result<String> {
    // Assert absolute paths
    if path.starts_with('/') {
        // expand the absolute path as a relative path, then perform the checks on it.
        let path = expand(&path[1 ..])?;
        if path.starts_with("..") {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "Cannot have a relative path beyond the root directory"
            ));
        }
        return Ok(join("/", &path));
    }
    // Expand the relative path
    let mut expanded = Vec::new();
    for part in path.split('/') {
        if part != "." && part != ".." {
            expanded.push(part);
            continue;
        }
        if part == ".." {
            if expanded.is_empty() || expanded[expanded.len() - 1] == ".." {
                expanded.push("..");
            } else {
                expanded.pop();
            }
        }
    }
    Ok(expanded.join("/"))
}

fn load_mimetypes(path: &str) -> io::Result<HashMap<String, String>> {
    let mimetypes = std::fs::read_to_string(path)?;
    Ok(toml::from_str(&mimetypes)?)
}

/// Get the mimetype of a file from the filename
/// 
/// This will load any available mime types from the 'mime-types.toml' file
pub fn get_mimetype(path: &str) -> &str {
    lazy_static! {
        static ref CONFIG_DIR: String = std::env::args().nth(1).expect("A config directory was expected");
        static ref MIMETYPES: HashMap<String, String> = load_mimetypes(&join(&CONFIG_DIR, "mime-types.toml")).unwrap();
    }
    let (_name, ext) = splitext(path);
    if ext.is_empty() {
        return MIMETYPES.get("bin").unwrap().as_str();
    }
    let ext = &ext[1 ..];
    return MIMETYPES.get(ext).or(MIMETYPES.get("bin")).unwrap().as_str();
}


#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_join() {
        assert_eq!(join("foo", "bar"), "foo/bar");
        assert_eq!(join("foo/", "bar"), "foo/bar");
        assert_eq!(join("foo", "/bar"), "foo/bar");
        assert_eq!(join("foo/", "/bar"), "foo/bar");
    }

    #[test]
    fn test_join_macro() {
        assert_eq!(join!("foo", "bar"), "foo/bar");
        assert_eq!(join!("foo", "bar", "cheese"), "foo/bar/cheese");
        assert_eq!(join!("foo", "bar", "cheese", "qwerty"), "foo/bar/cheese/qwerty");
    }

    #[test]
    fn test_splitext() {
        let (name, ext) = splitext("asdf.qwer");
        assert_eq!(name, "asdf");
        assert_eq!(ext, ".qwer");
        let (name, ext) = splitext("asdf");
        assert_eq!(name, "asdf");
        assert_eq!(ext, "");
        let (name, ext) = splitext(".asdf");
        assert_eq!(name, "");
        assert_eq!(ext, ".asdf");
    }

    #[test]
    fn test_basename() {
        assert_eq!(basename("asdf/qwerty"), "qwerty");
        assert_eq!(basename("/qwerty"), "qwerty");
        assert_eq!(basename("qwerty"), "qwerty");
        assert_eq!(basename("asdf/qwerty/"), "qwerty");
    }

    #[test]
    fn test_dirname() {
        assert_eq!(dirname("asdf/qwerty"), "asdf/");
        assert_eq!(dirname("/qwerty"), "/");
        assert_eq!(dirname("qwerty"), "");
        assert_eq!(dirname("asdf/qwerty/"), "asdf/");
    }

    #[test]
    fn test_expand() {
        assert_eq!(expand("/foo/bar").unwrap(), "/foo/bar");
        assert_eq!(expand("/foo/bar/").unwrap(), "/foo/bar/");
        assert_eq!(expand("/foo/bar/../").unwrap(), "/foo/");
        assert_eq!(expand("/foo/bar/../../").unwrap(), "/");
        assert_eq!(expand("foo/bar/../../../").unwrap(), "../");
        expand("/foo/bar/../../../").expect_err("Expected an error with too many updirs");
    }

    #[test]
    fn test_mimetypes() {
        assert_eq!(get_mimetype("foo.js"), "text/javascript");
        assert_eq!(get_mimetype("foo.gmi"), "text/gemini");
        assert_eq!(get_mimetype("foo"), "application/octet-stream");
    }
}