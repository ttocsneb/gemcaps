use std::{collections::{BinaryHeap, HashMap}, hash::Hash, time::{Instant, Duration}};

#[derive(Clone, Eq)]
struct ExpiringKey<K> {
    expires: Instant,
    key: K,
}

impl<K: Eq> Ord for ExpiringKey<K> {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.expires.cmp(&other.expires)
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
    where K: Hash + Eq + Ord + Clone {
    
    /// Create a new cache
    pub fn new() -> Self {
        Cache {
            items: HashMap::new(),
            queue: BinaryHeap::new(),
        }
    }

    /// Check if the cache contains the key
    pub fn contains_key(&self, key: &K) -> bool {
        self.items.contains_key(key)
    }
    /// Get an item from the cache
    pub fn get(&self, key: &K) -> Option<&V> {
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
    pub fn remove(&mut self, key: &K) {
        // Make sure that no extra work is done if it doesn't need to be
        if !self.contains_key(key) {
            return;
        }
        let temp = self.queue.clone();
        self.queue.clear();
        for item in temp.iter().filter(|x| x.key.ne(key)) {
            self.queue.push(item.to_owned());
        }
        self.items.remove(key);
    }
    /// Remove all items from the cache
    pub fn clear(&mut self) {
        self.items.clear();
        self.queue.clear();
    }
}
