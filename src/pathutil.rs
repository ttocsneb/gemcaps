use std::{collections::HashMap, io, path::{Component, Path, PathBuf}};

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
/// ```rust
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
pub fn abspath(base: impl AsRef<Path>, path: impl AsRef<Path>) -> PathBuf {
    let base = base.as_ref();
    let path = path.as_ref();
    if path.is_absolute() {
        return path.to_path_buf();
    }
    base.join(path)
}

/// Traverse a path that contains ParentDir (..) safely.
/// 
/// If the path traversal leaves the current path, then an error is returned.
/// Otherwise the traversed path is returned
/// 
/// # Example
/// 
/// ```rust
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


/// Split the name from the extension
/// 
/// # Example
/// 
/// ```rust
/// let (name, ext) = splitext("foo.bar");
/// assert_eq!(name, "foo");
/// assert_eq!(ext, ".bar");
/// ```
pub fn splitext<'a>(path: &'a str) -> (&'a str, &'a str) {
    lazy_static! {
        static ref RE: Regex = Regex::new(r"(.*)(\..*)").unwrap();
    }
    let groups = match RE.captures(path) {
        Some(groups) => groups,
        None => {
            return (path, "");
        }
    };
    let name = groups.get(1).map_or("", |m| m.as_str());
    let ext = groups.get(2).map_or("", |m| m.as_str());
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
pub fn basename<'a>(path: &'a str) -> &'a str {
    lazy_static! {
        static ref RE: Regex = Regex::new(r"/([^/]+)$").unwrap();
    }
    if path.ends_with('/') {
        return basename(&path[0 .. path.len() - 1]);
    }
    match RE.captures(path) {
        Some(m) => m.get(1).map_or("", |m| m.as_str()),
        None => path,
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

    let mimetypes = std::fs::read_to_string(join(&args.config, "mime-types.toml"))?;
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