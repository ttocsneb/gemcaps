use std::{collections::HashMap, io};

use regex::Regex;
use lazy_static::lazy_static;

/// Check if the path is an absolute path or not
/// 
/// This does work with windows paths
/// 
/// # Example
/// 
/// ```rust
/// assert_true!(is_abs("/foo/bar"));
/// assert_true!(is_abs("C:\\foo\\bar"));
/// assert_false!(is_abs("foo/bar"));
/// ```
pub fn is_abs(path: &str) -> bool {
    lazy_static! {
        static ref RE: Regex = Regex::new(r"^([a-zA-Z]:|/)").unwrap();
    }
    RE.is_match(path)
}

fn ends_with_slash(path: &str) -> bool {
    path.ends_with('/') || path.ends_with('\\')
}

fn starts_with_slash(path: &str) -> bool {
    path.starts_with('/') || path.starts_with('\\')
}


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
    if ends_with_slash(path_a) {
        return join(&path_a[0 .. path_a.len() - 1], path_b);
    }
    if starts_with_slash(path_b) {
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
    if is_abs(path) {
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
        static ref RE: Regex = Regex::new(r"[/\\]([^/\\]+)$").unwrap();
    }
    if ends_with_slash(path) {
        return basename(&path[0 .. path.len() - 1]);
    }
    match RE.captures(path) {
        Some(m) => m.get(1).map_or("", |m| m.as_str()),
        None => path,
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
pub fn dirname<'a>(path: &'a str) -> &'a str {
    lazy_static! {
        static ref RE: Regex = Regex::new(r"^.*[/\\]").unwrap();
    }
    if ends_with_slash(path) {
        return dirname(&path[0 .. path.len() - 1]);
    }
    match RE.find(path) {
        Some(m) => m.as_str(),
        None => "",
    }
}

/// Run dirname multiple times
/// 
/// # Example
/// 
/// ```rust
/// let dir = dirnames("/foo/bar/cheese", 2);
/// assert_eq!(dir, "/foo/")
/// ```
/// 
pub fn dirnames<'a>(path: &'a str, dirs: u32) -> &'a str {
    let mut result = path;
    for _ in 0 .. dirs {
        result = dirname(result);
    }
    result
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
        static ref RE: Regex = Regex::new(r"^([a-zA-z]:[/\\]|/)(.*)").unwrap();
    }
    // Assert absolute paths
    if let Some(captures) = RE.captures(path) {
        // expand the absolute path as a relative path, then perform the checks on it.
        let path = expand(captures.get(2).unwrap().as_str())?;
        if path.starts_with("..") {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "Cannot have a relative path beyond the root directory"
            ));
        }
        return Ok(join(captures.get(1).unwrap().as_str(), &path));
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
    let config_dir = std::env::args().nth(1).unwrap_or_else(|| String::from("example"));
    let mimetypes = std::fs::read_to_string(join(&config_dir, "mime-types.toml"))?;
    Ok(toml::from_str(&mimetypes)?)
}

/// Get the mimetype of a file from the filename
/// 
/// This will load any available mime types from the 'mime-types.toml' file
pub fn get_mimetype(path: &str) -> &str {
    lazy_static! {
        static ref MIMETYPES: HashMap<String, String> = load_mimetypes().unwrap();
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
    fn test_dirnames() {
        assert_eq!(dirnames("asdf/qwerty", 1), "asdf/");
        assert_eq!(dirnames("asdf/qwerty", 2), "");
        assert_eq!(dirnames("/asdf/qwerty", 2), "/");
    }

    #[test]
    fn test_expand() {
        assert_eq!(expand("/foo/bar").unwrap(), "/foo/bar");
        assert_eq!(expand("/foo/bar/").unwrap(), "/foo/bar/");
        assert_eq!(expand("/foo/bar/../").unwrap(), "/foo/");
        assert_eq!(expand("C:\\foo\\bar\\..\\").unwrap(), "C:/foo/");
        assert_eq!(expand("/foo/bar/../../").unwrap(), "/");
        assert_eq!(expand("foo/bar/../../../").unwrap(), "../");
        expand("/foo/bar/../../../").expect_err("Expected an error with too many updirs");
        expand("C:\\foo\\bar\\..\\..\\..\\").expect_err("Expected an error with too many updirs");
    }

    #[test]
    fn test_mimetypes() {
        assert_eq!(get_mimetype("foo.js"), "text/javascript");
        assert_eq!(get_mimetype("foo.gmi"), "text/gemini");
        assert_eq!(get_mimetype("foo"), "application/octet-stream");
    }
}