#include "cache.hpp"

#include "main.hpp"

#include <vector>

#include <uv.h>

using std::string;
using std::vector;
using std::make_shared;
using std::shared_ptr;


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
    uv_handle_set_data((uv_handle_t *)&timer, this);
    uv_timer_init(loop, &timer);
}

CacheInfo::~CacheInfo() {
    uv_timer_stop(&timer);
}

void CacheInfo::setTimout(unsigned int time) {
    uv_timer_start(&timer, cache_info_timer_cb, time, time);
    timer_active = true;
}

void CacheInfo::stopTimer() {
    uv_timer_stop(&timer);
    timer_active = false;
}

unsigned int CacheInfo::getTime() const {
    if (!timer_active) {
        return -1;
    }
    return uv_timer_get_due_in(&timer);
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
    string name = cache.begin()->second->getName();
    unsigned int shortest =cache.begin()->second->getTime();
    for (auto pair : cache) {
        unsigned int t = pair.second->getTime();
        if (t < shortest) {
            name = pair.second->getName();
            shortest = t;
        }
    }
    invalidate(name);
}

void Cache::_onCacheTimer(CacheInfo *info) {
    invalidate(info->getName());
}

shared_ptr<CacheInfo> Cache::_get(const string &name) {
    if (!cache.count(name)) {
        std::pair<string, shared_ptr<CacheInfo>> p(name, make_shared<CacheInfo>(loop, this, name));
        cache.insert(p);
    }
    return cache.at(name);
}

void Cache::loading(const string &name) {
    auto c = _get(name);
    c->setLoading();
    c->stopTimer();
}

void Cache::add(const string &name, CachedData data) {
    LOG_DEBUG("Adding " << name);
    if (max_size > 0) {
        while (size + data.body.length() > max_size) {
            _remove_old_cache();
        }
    }
    auto c = _get(name);
    c->setData(data);
    size += c->getSize();
    if (data.lifetime) {
        c->setTimout(data.lifetime);
    } else {
        c->stopTimer();
    }
    c->setLoaded();
}

void Cache::clear() {
    cache.clear();
}

void Cache::invalidate(const string &name) {
    LOG_DEBUG("invalidating " << name);
    if (cache.count(name)) {
        if (cache.at(name)->isLoaded()) {
            size -= cache.at(name)->getSize();
        }
        cache.erase(name);
    }
}

bool Cache::getNotified(const string &name, CacheReadyCB callback, void *ctx) {
    if (!cache.count(name)) {
        return false;
    }
    auto c = cache.at(name);
    if (c->isLoaded()) {
        // If the data is already loaded, just call the callback immediately
        callback(c->getData(), ctx);
        return true;
    }
    c->addCallback(callback, ctx);
    return true;
}

bool Cache::isLoading(const string &name) const {
    if (cache.count(name)) {
        return cache.at(name)->isLoading();
    }
    return false;
}

bool Cache::isLoaded(const string &name) const {
    if (cache.count(name)) {
        return cache.at(name)->isLoaded();
    }
    return false;
}

const CachedData &Cache::get(const string &name) const {
    return cache.at(name)->getData();
}