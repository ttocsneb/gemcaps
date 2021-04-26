#include "cache.hpp"

#include <vector>

using std::string;
using std::vector;

unsigned int sizeofCache(CacheData *data) {
    unsigned int size = 0;
    size += data->body.size();
    size += data->meta.size();
    return size;
}

void Cache::_clear_old_cache() {
    vector<string> to_delete;
    for (auto &c : cache) {
        CacheInfo &info = cache_info[c.first];
        // TODO: if (info.cache_start - now > c.lifetime) {
        if (true) {
            used_data -= sizeofCache(&c.second);
            to_delete.push_back(c.first);
        }
    }
    for (string &d : to_delete) {
        cache.erase(d);
        cache_info.erase(d);
    }
}

void Cache::loading(const string &name) {
    if (!cache_info.count(name)) {
        cache_info[name].cache_start = 0;
        cache_info[name].callbacks.clear();
    }
    cache_info.at(name).loading = true;
}

void Cache::add(const string &name, CacheData data) {
    if (cache.count(name)) {
        used_data -= sizeofCache(&cache.at(name));
    }
    used_data += sizeofCache(&data);
    cache[name] = data;
    cache_info[name].loading = false;
    //TODO: cache_info[name].cache_start = now

    // Call the callbacks
    for (auto callback : cache_info[name].callbacks) {
        callback.first(data, callback.second);
    }
    cache_info[name].callbacks.clear();

    _clear_old_cache();
}

void Cache::clear() {
    cache.clear();
    cache_info.clear();
    used_data = 0;
}

bool Cache::isLoading(const string &name) const {
    if (cache_info.count(name)) {
        return cache_info.at(name).loading;
    }
    return false;
}

bool Cache::contains(const string &name) const {
    return cache.count(name);
}

const CacheData &Cache::get(const string &name) const {
    return cache.at(name);
}