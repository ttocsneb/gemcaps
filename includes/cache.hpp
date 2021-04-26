#ifndef __GEMCAPS_CACHE__
#define __GEMCAPS_CACHE__

#include <string>
#include <map>
#include <set>

typedef struct {
    std::string meta;
    int response;
    std::string body;
    unsigned int lifetime;
} CacheData;

unsigned int sizeofCache(CacheData *data);

typedef void (*CacheReady)(const CacheData &data, void *);

class Cache {
private:
    typedef struct {
        int cache_start;
        std::map<CacheReady, void *> callbacks;
        bool loading;
    } CacheInfo;

    const unsigned int max_size;
    unsigned int used_data;

    std::map<std::string, CacheData> cache;
    std::map<std::string, CacheInfo> cache_info;

    void _clear_old_cache();
public:
    /**
     * Create a cache with a maximum memory limit
     * 
     * @param max_size the maximum number of bytes to store in the cache (A value of 0 means no limit)
     */
    Cache(unsigned int max_size = 0)
        : max_size(max_size) {}
    
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
     * Get a notification when the cache is ready
     * 
     * @param name name of the cache
     * @param callback function to call
     * @param argument to pass to the function when called
     * 
     * @return whether the function will be notified
     */
    bool getNotified(const std::string &name, CacheReady callback, void *argument = nullptr);

    /**
     * Check if a cache is being loaded
     * 
     * @param name name of the cache
     * 
     * @return whether the cache is being loaded
     */
    bool isLoading(const std::string &name) const;
    /**
     * Check if the cache exists
     * 
     * @param name name of the cache
     * 
     * @return whether the cache is available
     */
    bool contains(const std::string &name) const;
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