use std::borrow::Borrow;
use std::collections::{BinaryHeap, HashMap};
use std::hash::Hash;
use std::time::{Instant, Duration};

#[derive(Clone, Eq)]
struct ExpiringKey<K> {
    expires: Instant,
    key: K,
}

impl<K: Eq> Ord for ExpiringKey<K> {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        other.expires.cmp(&self.expires)
    }
}

impl<K: Eq> PartialOrd for ExpiringKey<K> {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<K: PartialEq> PartialEq for ExpiringKey<K> {
    fn eq(&self, other: &Self) -> bool {
        (self.expires, &self.key) == (other.expires, &other.key)
    }
}

impl<K> ExpiringKey<K> {
    fn new(key: K, lifetime: f32) -> Self {
        ExpiringKey {
            expires: Instant::now() + Duration::from_secs_f32(lifetime),
            key,
        }
    }
    fn is_expired(&self) -> bool {
        let now = Instant::now();
        self.expires.checked_duration_since(now) == None
    }
}

pub struct Cache<K, V> {
    items: HashMap<K, V>,
    queue: BinaryHeap<ExpiringKey<K>>,
}

impl<K, V> Cache<K, V> 
    where K: Hash + Eq + Clone {
    
    /// Create a new cache
    pub fn new() -> Self {
        Cache {
            items: HashMap::new(),
            queue: BinaryHeap::new(),
        }
    }

    /// Check if the cache contains the key
    pub fn contains_key<Q>(&self, key: &Q) -> bool 
        where
            Q: ?Sized,
            K: Borrow<Q>,
            Q: Hash + Eq {
        self.items.contains_key(key)
    }
    /// Get an item from the cache
    pub fn get<Q>(&self, key: &Q) -> Option<&V>
        where
            Q: ?Sized,
            K: Borrow<Q>,
            Q: Hash + Eq {
        self.items.get(key)
    }
    /// Insert an item into the cache
    /// 
    /// The item you insert will have a key and a lifetime. The lifetime is the
    /// number of seconds until the item is expired.
    pub fn insert(&mut self, key: K, value: V, lifetime: f32) {
        self.remove(&key);
        self.queue.push(ExpiringKey::new(key.clone(), lifetime));
        self.items.insert(key, value);
    }
    /// Clean up any items that have expired
    /// 
    /// Note that this is the only way to remove any expired items from the cache.
    pub fn clean_up(&mut self) -> u32 {
        let mut cleaned_items = 0;
        while let Some(next) = self.queue.peek() {
            if !next.is_expired() {
                break;
            }
            self.items.remove(&next.key);
            self.queue.pop();
            cleaned_items += 1;
        }
        cleaned_items
    }
    /// Remove a specific item from the cache
    /// 
    /// If the item isn't in the queue, then no change will be made
    pub fn remove<Q>(&mut self, key: &Q)
        where
            Q: ?Sized,
            K: Borrow<Q>,
            Q: Hash + Eq {
        // Note: I wish that I could borrow the queue as a vector, run the 
        // experimental method `drain_filter` to remove the items I don't want,
        // then convert that vector back into the queue.
        // I don't think that borrowing this way will be possible, but it would
        // be better if I could at least run drain_filter so that I don't end
        // up making two copies of the queue

        // Make sure that no extra work is done if it doesn't need to be
        if !self.contains_key(key) {
            return;
        }
        let temp = self.queue.clone();
        self.queue.clear();
        for item in temp.iter().filter(|x| key.ne(x.key.borrow())) {
            self.queue.push(item.to_owned());
        }
        self.items.remove(key);
    }
    /// Remove all items from the cache
    pub fn clear(&mut self) {
        self.items.clear();
        self.queue.clear();
    }
    /// Check if the cache is empty
    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }
    /// Get the number of items in the cache
    pub fn len(&self) -> usize {
        self.items.len()
    }
}

#[cfg(test)]
mod test {
    use std::thread::sleep;

    use super::*;

    #[test]
    fn test_insert() {
        let mut cache = Cache::new();
        cache.insert("foo".to_string(), "bar".to_string(), 5.0);
        assert_eq!(cache.contains_key("foo"), true);
        assert_eq!(cache.get("foo").unwrap(), "bar");
    }

    #[test]
    fn test_clear() {
        let mut cache = Cache::new();
        cache.insert("foo".to_string(), "bar".to_string(), 5.0);
        assert_eq!(cache.is_empty(), false);
        assert_eq!(cache.len(), 1);
        cache.clear();
        assert_eq!(cache.is_empty(), true);
        assert_eq!(cache.len(), 0);
    }

    #[test]
    fn test_cleanup() {
        let mut cache = Cache::new();
        cache.insert("foo".to_string(), "bar".to_string(), 0.0);
        cache.insert("bar".to_string(), "cheese".to_string(), 5.0);
        sleep(Duration::from_secs_f32(0.1));
        assert_eq!(cache.is_empty(), false);
        assert_eq!(cache.len(), 2);
        assert_eq!(cache.clean_up(), 1);
        assert_eq!(cache.is_empty(), false);
        assert_eq!(cache.len(), 1);
    }

    #[test]
    fn test_replace() {
        let mut cache = Cache::new();
        cache.insert("foo".to_string(), "bar".to_string(), 0.0);
        cache.insert("bar".to_string(), "cheese".to_string(), 5.0);
        cache.insert("foo".to_string(), "yeet".to_string(), 5.0);
        sleep(Duration::from_secs_f32(0.1));
        assert_eq!(cache.is_empty(), false);
        assert_eq!(cache.len(), 2);
        assert_eq!(cache.clean_up(), 0);
        assert_eq!(cache.is_empty(), false);
        assert_eq!(cache.len(), 2);
    }

    #[test]
    fn test_remove() {
        let mut cache = Cache::new();
        cache.insert("foo".to_string(), "bar".to_string(), 5.0);
        cache.insert("bar".to_string(), "cheese".to_string(), 5.0);
        sleep(Duration::from_secs_f32(0.1));
        assert_eq!(cache.is_empty(), false);
        assert_eq!(cache.len(), 2);
        cache.remove("foo");
        assert_eq!(cache.is_empty(), false);
        assert_eq!(cache.len(), 1);
    }
}