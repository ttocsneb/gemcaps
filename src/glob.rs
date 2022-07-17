
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Glob {
    matchers: Vec<String>,
}

impl Glob {
    pub fn new(glob: impl AsRef<str>) -> Self {
        let mut matchers = vec![];

        let mut glob = glob.as_ref();
        while let Some(pos) = glob.find("*") {
            matchers.push(glob[..pos].to_string());
            let mut chars = glob[pos..].chars();
            chars.next();
            glob = chars.as_str();
        }
        matchers.push(glob.to_string());

        Self {
            matchers
        }
    }

    pub fn matches(&self, text: impl AsRef<str>) -> bool {
        let mut text = text.as_ref();
        if !text.starts_with(&self.matchers[0]) {
            return false;
        }

        for matcher in &self.matchers {
            if let Some(pos) = text.find(matcher) {
                text = &text[matcher.len() + pos..];
            } else {
                return false;
            }
        }

        if self.matchers.last().unwrap().is_empty() && self.matchers.len() > 1 {
            return true;
        }
        text.is_empty()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_glob_new() {
        let glob = Glob::new("Hello");
        assert_eq!(glob.matchers, vec!["Hello"]);

        let glob = Glob::new("*Hello");
        assert_eq!(glob.matchers, vec!["", "Hello"]);

        let glob = Glob::new("Hello*");
        assert_eq!(glob.matchers, vec!["Hello", ""]);
    }

    #[test]
    fn test_matches() {
        let glob = Glob::new("Hello");
        assert_eq!(glob.matches("Hello"), true, "'Hello' != 'Hello'");
        assert_eq!(glob.matches("hello"), false, "'hello' == 'Hello'");
        assert_eq!(glob.matches("FooHello"), false, "'FooHello' == 'Hello'");
        assert_eq!(glob.matches("HelloFoo"), false, "'HelloFoo' == 'Hello'");
        assert_eq!(glob.matches("FooHelloBar"), false, "'FooHelloBar' == 'Hello'");
        assert_eq!(glob.matches(""), false, "'' == 'Hello'");

        let glob = Glob::new("*Hello");
        assert_eq!(glob.matches("Hello"), true, "'Hello' != '*Hello'");
        assert_eq!(glob.matches("FooHello"), true, "'FooHello' != '*Hello'");
        assert_eq!(glob.matches("HelloFoo"), false, "'HelloFoo' == '*Hello'");
        assert_eq!(glob.matches("FooHelloBar"), false, "'FooHelloBar' == '*Hello'");
        assert_eq!(glob.matches(""), false, "'' == '*Hello'");

        let glob = Glob::new("Hello*");
        assert_eq!(glob.matches("Hello"), true, "'Hello' != 'Hello*'");
        assert_eq!(glob.matches("FooHello"), false, "'FooHello' == 'Hello*'");
        assert_eq!(glob.matches("HelloFoo"), true, "'HelloFoo' != 'Hello*'");
        assert_eq!(glob.matches("FooHelloBar"), false, "'FooHelloBar' == 'Hello*'");
        assert_eq!(glob.matches(""), false, "'' == 'Hello*'");

        let glob = Glob::new("*Hello*");
        assert_eq!(glob.matches("Hello"), true, "'Hello' != '*Hello*'");
        assert_eq!(glob.matches("FooHello"), true, "'FooHello' != '*Hello*'");
        assert_eq!(glob.matches("HelloFoo"), true, "'HelloFoo' != '*Hello*'");
        assert_eq!(glob.matches("FooHelloBar"), true, "'FooHelloBar' != '*Hello*'");
        assert_eq!(glob.matches(""), false, "'' == '*Hello*'");
    }

    #[test]
    fn test_empty_glob() {
        let glob = Glob::new("*");
        assert_eq!(glob.matches("Hello"), true, "'Hello' != '*'");
        assert_eq!(glob.matches(""), true, "'' != '*'");

        let glob = Glob::new("");
        assert_eq!(glob.matches("Hello"), false, "'Hello' == ''");
        assert_eq!(glob.matches(""), true, "'' != ''");
    }
}
