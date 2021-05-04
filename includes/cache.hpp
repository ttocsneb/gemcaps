#ifndef __GEMCAPS_CACHE__
#define __GEMCAPS_CACHE__

#include <string>
#include <map>
#include <set>
#include <memory>
#include <queue>

#include <uv.h>

typedef struct {
    std::string meta;
    int response;
    std::string body;
    unsigned int lifetime;
} CacheData;

typedef void (*CacheReadyCB)(const CacheData &data, void *);

void cache_info_timer_cb(uv_timer_t *handle);

class Cache;

class CacheInfo {
private:
    uv_timer_t timer;
    std::map<CacheReadyCB, void *> callbacks;
    bool loading;
    bool timer_active;
    bool ready;

    Cache *manager;

    CacheData data;
    std::string name;
public:
    CacheInfo(uv_loop_t *loop, Cache *manager, std::string name);
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
    const CacheData &getData() const { return data; }
    /**
     * Set the data in the cache
     * 
     * @param data
     */
    void setData(const CacheData &data) { this->data = data; }

    /**
     * Get the name of the cache
     * 
     * @return name
     */
    const std::string &getName() const { return name; }

    /**
     * Called when the timer has run out
     */
    void _timeout();

    /**
     * Get the size of the cache in bytes
     * 
     * @return size
     */
    unsigned int getSize() const { return data.body.length(); }
};


class Cache {
private:


    uv_loop_t *loop;

    const unsigned int max_size;

    std::map<std::string, std::shared_ptr<CacheInfo>> cache;

    unsigned int size;

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
    std::shared_ptr<CacheInfo> _get(const std::string &name);

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
     * @param name name of the cache
     */
    void loading(const std::string &name);
    /**
     * Add the data to the cache
     * 
     * @param name name of the cache
     * @param data data to add
     */
    void add(const std::string &name, CacheData data);

    /**
     * Clear all data in the cache
     */
    void clear();
    /**
     * Invalidate a cache
     * 
     * This can be useful if we know that the data has been updated
     * 
     * @param name name of the cache
     */
    void invalidate(const std::string &name);

    /**
     * Get a notification when the cache is ready
     * 
     * @param name name of the cache
     * @param callback function to call
     * @param argument to pass to the function when called
     * 
     * @return whether the function will be notified
     */
    bool getNotified(const std::string &name, CacheReadyCB callback, void *argument = nullptr);

    /**
     * Check if a cache is being loaded
     * 
     * @param name name of the cache
     * 
     * @return whether the cache is being loaded
     */
    bool isLoading(const std::string &name) const;
    /**
     * Check if the cache is loaded
     * 
     * @param name name of the cache
     * 
     * @return whether the cache is loaded
     */
    bool isLoaded(const std::string &name) const;
    /**
     * Retrieve the cache
     * 
     * @param name
     * 
     * @return the data in the cache
     */
    const CacheData &get(const std::string &name) const;
};

#endif