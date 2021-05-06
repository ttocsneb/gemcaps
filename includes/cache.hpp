#ifndef __GEMCAPS_CACHE__
#define __GEMCAPS_CACHE__

#include <string>
#include <map>
#include <set>
#include <memory>
#include <queue>

#include <uv.h>

typedef struct CachedData {
    std::string meta;
    int response;
    std::string body;
    unsigned int lifetime;

    std::string generateResponse() const;
} CachedData;

typedef struct CacheKey {
    size_t hash;
    std::string name;

    bool operator<(const CacheKey &rhs) const;
} CacheKey;


void cache_info_timer_cb(uv_timer_t *handle);

class Cache;

typedef void (*CacheReadyCB)(const CachedData &data, Cache *cache, void *arg);

class CacheInfo {
private:
    uv_timer_t timer;
    std::map<CacheReadyCB, void *> callbacks;
    bool loading;
    bool timer_active;
    bool ready;

    Cache *manager;

    CachedData data;
    CacheKey key;
public:
    CacheInfo(uv_loop_t *loop, Cache *manager, CacheKey key);
    ~CacheInfo();

    /**
     * Set the timeout for the timer
     * 
     * @param time time in ms before calling Cache::_onCacheTimer()
     */
    void setTimout(unsigned int time);
    /**
     * Stop the timer
     */
    void stopTimer();
    /**
     * Get the amount of time left in the timer
     * 
     * @return time
     */
    unsigned int getTime() const;
    /**
     * Add a callback for notifying when the cache is ready
     * 
     * @param cb callback
     * @param ctx context
     */
    void addCallback(CacheReadyCB cb, void *ctx) { callbacks.insert_or_assign(cb, ctx); }

    /**
     * Set the state of the cache to be loading the cache
     */
    void setLoading();
    /**
     * Cancel the cache load.
     * 
     * If there are requests waiting for the cache, then the first waiting
     * cache will be notified, and it should start creating the request.
     * 
     * If there are no requests, then the cache will be invalidated
     */
    void cancelLoading();
    /**
     * Set the state of the cache to be loaded and notify all callbacks that it is loaded
     */
    void setLoaded();
    /**
     * Check whether the cache is loading
     * 
     * @return whether it is loading
     */
    bool isLoading() const { return loading; }
    /**
     * Check if the cache is loaded
     * 
     * @return whether it is loaded
     */
    bool isLoaded() const { return ready; }

    /**
     * Get the data in the cache
     * 
     * @return the cache data
     */
    const CachedData &getData() const { return data; }
    /**
     * Set the data in the cache
     * 
     * @param data
     */
    void setData(const CachedData &data) { this->data = data; }

    /**
     * Get the name of the cache
     * 
     * @return name
     */
    const CacheKey &getKey() const { return key; }

    /**
     * Called when the timer has run out
     */
    void _timeout();

    /**
     * Get the size of the cache in bytes
     * 
     * @return size
     */
    size_t getSize() const { return data.body.length(); }
};


class Cache {
public:
private:
    uv_loop_t *loop;

    const unsigned int max_size;

    std::map<CacheKey, std::shared_ptr<CacheInfo>> cache;

    unsigned int size = 0;

    /**
     * Remove the cache closest to timing out
     */
    void _remove_old_cache();
protected:
    /**
     * Called when a cach times out
     * 
     * @param info cache that timed out
     */
    void _onCacheTimer(CacheInfo *info);

    /**
     * Get or create a cache
     * 
     * @param name cache name
     * 
     * @return cache
     */
    std::shared_ptr<CacheInfo> _get(const CacheKey &key);

    friend CacheInfo;
public:
    /**
     * Create a cache with a maximum memory limit
     * 
     * @param max_size the maximum number of bytes to store in the cache (A value of 0 means no limit)
     */
    Cache(uv_loop_t *loop, unsigned int max_size = 0)
        : loop(loop),
          max_size(max_size) {}
    
    /**
     * Let the cache know that the name is actively being loaded
     * 
     * This is to make certain that only one job is made to create the cache
     * 
     * @param key key
     */
    void loading(const CacheKey &key);
    /**
     * Let the cache know that the cache is no longer being loaded
     * 
     * @param key key
     */
    void cancel(const CacheKey &key);
    /**
     * Add the data to the cache
     * 
     * @param key key
     * @param data data to add
     */
    void add(const CacheKey &key, CachedData data);

    /**
     * Clear all data in the cache
     */
    void clear();
    /**
     * Invalidate a cache
     * 
     * This can be useful if we know that the data has been updated
     * 
     * @param key key
     */
    void invalidate(const CacheKey &key);

    /**
     * Get a notification when the cache is ready
     * 
     * @param key key
     * @param callback function to call
     * @param argument to pass to the function when called
     * 
     * @return whether the function will be notified
     */
    bool getNotified(const CacheKey &key, CacheReadyCB callback, void *argument = nullptr);

    /**
     * Check if a cache is being loaded
     * 
     * @param key key
     * 
     * @return whether the cache is being loaded
     */
    bool isLoading(const CacheKey &key) const;
    /**
     * Check if the cache is loaded
     * 
     * @param key key
     * 
     * @return whether the cache is loaded
     */
    bool isLoaded(const CacheKey &key) const;
    /**
     * Retrieve the cache
     * 
     * @param key key
     * 
     * @return the data in the cache
     */
    const CachedData &get(const CacheKey &key) const;
};

#endif