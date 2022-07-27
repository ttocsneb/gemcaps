use std::{collections::HashMap, io, path::{Component, Path, PathBuf}, borrow::Cow};

use regex::Regex;
use lazy_static::lazy_static;

use crate::args;

/// Join two paths together.
/// 
/// This will make sure that there are no duplicate '/' where the two paths are
/// joined.
/// 
/// # Example
/// 
/// ```
/// let joined = join("asdf", "qwerty");
/// assert_eq!(joined, "asdf/qwerty");
/// ```
/// 
pub fn join(path_a: &str, path_b: &str) -> String {
    if path_a.ends_with('/') {
        return join(&path_a[0 .. path_a.len() - 1], path_b);
    }
    if path_b.starts_with('/') {
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
/// ```
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

/// Traverse a path that contains ParentDir (..) safely.
/// 
/// If the path traversal leaves the current path, then an error is returned.
/// Otherwise the traversed path is returned
/// 
/// # Example
/// 
/// ```
/// let traversed = traversal_safe("/foo/bar/../cheese").unwrap();
/// assert_eq!(traversed, "/foo/cheese");
/// traversal_safe("/foo/bar/../../..").expect_err("Should return an error");
/// ```
pub fn traversal_safe(path: impl AsRef<Path>) -> Result<PathBuf, std::io::Error> {
    let path = path.as_ref();
    let mut result = Vec::new();
    for component in path.components() {
        if component == Component::ParentDir {
            match result.pop() {
                Some(comp) => {
                    if comp == Component::RootDir {
                        return Err(std::io::Error::new(
                            std::io::ErrorKind::InvalidInput, 
                            "Cannot traverse outside of the root directory"
                        ));
                    }
                },
                None => {
                    return Err(std::io::Error::new(
                        std::io::ErrorKind::InvalidInput,
                        "Tried to traverse outside the local directory"
                    ));
                }
            }
        } else {
            result.push(component);
        }
    }
    let mut result_path = PathBuf::new();
    for component in result {
        result_path.push(component);
    }

    return Ok(result_path);
}

/// Get the final element in the path
/// 
/// # Example
/// 
/// ```
/// let name = basename("/foo/bar/cheese");
/// assert_eq!(name, "cheese");
/// ```
#[allow(dead_code)]
pub fn basename<'a>(path: &'a str) -> &'a str {
    lazy_static! {
        static ref RE: Regex = Regex::new(r"/([^/]+)/?$").unwrap();
    }
    match RE.captures(path) {
        Some(m) => m.get(1).map_or("", |m| m.as_str()),
        None => path,
    }
}

/// Get the parent directory of the path
/// 
/// If the path is a relative path, then up-dirs may be added to the path.
/// 
/// If the path is an absolute path, then None will be returned when trying to
/// get the parent of the root directory
/// 
/// # Example
/// 
/// ```
/// let parent = parent("/foo/bar/cheese");
/// assert_eq!(parent.unwrap(), "/foo/bar/");
/// let relative = parent("");
/// assert_eq!(parent.unwrap(), "../");
/// let relative = parent("/");
/// assert_eq!(parent, None);
/// 
/// ```
pub fn parent<'a>(path: &'a str) -> Option<Cow<'a, str>> {
    lazy_static! {
        static ref IS_UP: Regex = Regex::new(r"\.\./?$").unwrap();
        static ref BASE_DIR: Regex = Regex::new(r"[^/]+/?$").unwrap();
    }
    if path == "/" {
        return None;
    }

    if path.is_empty() || IS_UP.is_match(path) {
        let mut path = Cow::from(path);
        if !path.is_empty() && !path.ends_with('/') {
            path += "/";
        }
        path += "../";
        return Some(path);
    }
    Some(BASE_DIR.replace(path, ""))
}

/// Encode a path with percent encoding using [urlencoding]
/// 
/// urlencoding by default will encode '/' as '%2F' which should not be the
/// case when dealing with paths. This function will keep '/' as is while
/// encoding all other characters.
/// 
/// If no encoding is needed, then the original string is returned.
/// 
pub fn encode(path: &str) -> Cow<str> {
    let path = Cow::from(path);
    let mut changed = false;

    let parts: Vec<_> = path.split('/').map(|group| {
        let group = urlencoding::encode(&group);
        if let Cow::Owned(_) = &group {
            changed = true;
        }
        group
    }).collect();

    if changed {
        parts.join("/").into()
    } else {
        path
    }
}

/// Encode a path with percent encoding using [urlencoding]
/// 
/// urlencoding by default will encode '/' as '%2F' which should not be the
/// case when dealing with paths. This function will keep '/' as is while
/// encoding all other characters.
/// 
pub fn encode_binary(path: &[u8]) -> String {
    let parts: Vec<_> = path.split(|c| *c == b'/').map(
        |group| urlencoding::encode_binary(group)
    ).collect();

    parts.join("/").into()
}

/// Expand any '.' or '..' in a path
/// 
/// If the path is relative, then excess '..' will bleed over e.g.
/// 'foo/../../bar' -> '../bar'. However, if the path is absolute, an error is
/// returned when the path tries to beyond the root.
/// 
/// # Example
/// 
/// ```
/// let expanded = expand("foo/bar/../../../cheese/../yeet/").unwrap();
/// assert_eq!(expanded, "../yeet/");
/// ```
/// 
pub fn expand(path: &str) -> io::Result<String> {
    lazy_static! {
        static ref RE: Regex = Regex::new(r"^/(.*)").unwrap();
    }
    // Assert absolute paths
    if let Some(captures) = RE.captures(path) {
        // expand the absolute path as a relative path, then perform the checks on it.
        let path = expand(captures.get(1).unwrap().as_str())?;
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
    let path = path.replace("\\", "/");
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

fn load_mimetypes() -> io::Result<HashMap<String, String>> {
    let args = args();
    let config = if args.config == "." {
        "example"
    } else {
        &args.config
    };

    let mimetypes = std::fs::read_to_string(join(config, "mime-types.toml"))?;
    Ok(toml::from_str(&mimetypes)?)
}

/// Get the mimetype of a file from the filename
/// 
/// This will load any available mime types from the 'mime-types.toml' file
pub fn get_mimetype(path: impl AsRef<Path>) -> &'static str {
    lazy_static! {
        static ref MIMETYPES: HashMap<String, String> = load_mimetypes().unwrap();
    }
    let path = path.as_ref();
    if let Some(ext) = path.extension() {
        return MIMETYPES.get(match ext.to_str() {
            Some(ext) => ext,
            None => "bin"
        }).or_else(|| MIMETYPES.get("bin")).unwrap().as_str();
    }
    return MIMETYPES.get("bin").unwrap().as_str();
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
    fn test_basename() {
        assert_eq!(basename("asdf/qwerty"), "qwerty");
        assert_eq!(basename("/qwerty"), "qwerty");
        assert_eq!(basename("qwerty"), "qwerty");
        assert_eq!(basename("asdf/qwerty/"), "qwerty");
    }

    #[test]
    fn test_parent() {
        assert_eq!(parent("/asdf/qwerty").unwrap(), "/asdf/");
        assert_eq!(parent("asdf/qwerty").unwrap(), "asdf/");
        assert_eq!(parent("asdf/").unwrap(), "");
        assert_eq!(parent("/"), None);
        assert_eq!(parent("").unwrap(), "../");
        assert_eq!(parent("../").unwrap(), "../../");
    }

    #[test]
    fn test_encode() {
        assert_eq!(encode("foo/bar"), "foo/bar");
        assert_eq!(encode("foo cheese/bar"), "foo%20cheese/bar");
        assert_eq!(encode("foo cheese"), "foo%20cheese");
        assert_eq!(encode("/foo/bar/"), "/foo/bar/");
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

    #[test]
    fn test_traversal_safe() {
        let traversed = traversal_safe("/foo/bar/../cheese").unwrap();
        assert_eq!(traversed.to_str().unwrap(), "/foo/cheese");
        traversal_safe("/foo/bar/../../..").expect_err("Expected an error");
    }
}