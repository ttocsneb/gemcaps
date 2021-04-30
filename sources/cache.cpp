#include "cache.hpp"

#include <vector>

#include <uv.h>

using std::string;
using std::vector;
using std::make_shared;

unsigned int sizeofCache(CacheData *data) {
    unsigned int size = 0;
    size += data->body.size();
    size += data->meta.size();
    return size;
}

//////////////////// CacheInfo ////////////////////

void cache_info_timer_cb(uv_timer_t *handle) {
    CacheInfo *info = static_cast<CacheInfo *>(uv_handle_get_data((uv_handle_t *)handle));
    if (info != nullptr) {
        info->_timeout();
    }
}

CacheInfo::CacheInfo(uv_loop_t *loop, Cache *manager, string name)
        : manager(manager),
          loading(false),
          timer_active(false),
          ready(false),
          name(name) {
    timer = make_shared<uv_timer_t>();
    uv_handle_set_data((uv_handle_t *)timer.get(), this);
    uv_timer_init(loop, timer.get());
}

CacheInfo::~CacheInfo() {
    uv_timer_stop(timer.get());
}

void CacheInfo::setTimout(unsigned int time) {
    uv_timer_start(timer.get(), cache_info_timer_cb, time, time);
    timer_active = true;
}

void CacheInfo::stopTimer() {
    uv_timer_stop(timer.get());
    timer_active = false;
}

unsigned int CacheInfo::getTime() const {
    if (!timer_active) {
        return -1;
    }
    return uv_timer_get_due_in(timer.get());
}

void CacheInfo::setLoading() {
    loading = true;
    ready = false;
}

void CacheInfo::setLoaded() {
    loading = false;
    ready = true;
    for (auto pair : callbacks) {
        pair.first(data, pair.second);
    }
    callbacks.clear();
}

void CacheInfo::_timeout() {
    stopTimer();
    manager->_onCacheTimer(this);
}

//////////////////// Cache ////////////////////

void Cache::_remove_old_cache() {
    if (cache.empty()) {
        return;
    }
    string name = cache.begin()->second.getName();
    unsigned int shortest =cache.begin()->second.getTime();
    for (auto pair : cache) {
        unsigned int t = pair.second.getTime();
        if (t < shortest) {
            name = pair.second.getName();
            shortest = t;
        }
    }
    invalidate(name);
}

void Cache::_onCacheTimer(CacheInfo *info) {
    invalidate(info->getName());
}

CacheInfo &Cache::_get(const string &name) {
    if (!cache.count(name)) {
        cache[name] = CacheInfo(loop, this, name);
    }
    return cache.at(name);
}

void Cache::loading(const string &name) {
    CacheInfo &c = _get(name);
    c.setLoading();
    c.stopTimer();
}

void Cache::add(const string &name, CacheData data) {
    while (cache.size() >= max_size) {
        _remove_old_cache();
    }
    CacheInfo &c = _get(name);
    c.setData(data);
    if (data.lifetime) {
        c.setTimout(data.lifetime);
    } else {
        c.stopTimer();
    }
    c.setLoaded();
}

void Cache::clear() {
    cache.clear();
}

void Cache::invalidate(const string &name) {
    cache.erase(name);
}

bool Cache::getNotified(const string &name, CacheReadyCB callback, void *ctx) {
    if (!cache.count(name)) {
        return false;
    }
    CacheInfo &c = cache.at(name);
    if (c.isLoaded()) {
        // If the data is already loaded, just call the callback immediately
        callback(c.getData(), ctx);
        return true;
    }
    c.addCallback(callback, ctx);
    return true;
}

bool Cache::isLoading(const string &name) const {
    if (cache.count(name)) {
        return cache.at(name).isLoading();
    }
    return false;
}

bool Cache::isLoaded(const string &name) const {
    if (cache.count(name)) {
        return cache.at(name).isLoaded();
    }
    return false;
}

const CacheData &Cache::get(const string &name) const {
    return cache.at(name).getData();
}