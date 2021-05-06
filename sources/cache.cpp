#include "cache.hpp"

#include "main.hpp"

#include <vector>
#include <sstream>

#include <uv.h>

using std::string;
using std::vector;
using std::make_shared;
using std::shared_ptr;
using std::ostringstream;


bool CacheKey::operator<(const CacheKey &rhs) const {
    if (hash == rhs.hash) {
        return name < rhs.name;
    }
    return hash < rhs.hash;
}

string CachedData::generateResponse() const {
    ostringstream oss;
    oss << response << " " << meta << "\r\n";
    if (response >= 20 && response <= 29) {
        oss << body;
    }
    return oss.str();
}

//////////////////// CacheInfo ////////////////////

void cache_info_timer_cb(uv_timer_t *handle) {
    CacheInfo *info = static_cast<CacheInfo *>(uv_handle_get_data((uv_handle_t *)handle));
    if (info != nullptr) {
        info->_timeout();
    }
}

CacheInfo::CacheInfo(uv_loop_t *loop, Cache *manager, CacheKey key)
        : manager(manager),
          loading(false),
          timer_active(false),
          ready(false),
          key(key) {
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

void CacheInfo::cancelLoading() {
    loading = false;
    ready = false;
    if (!callbacks.empty()) {
        auto first = callbacks.begin();
        CacheReadyCB cb = first->first;
        void *ctx = first->second;
        callbacks.erase(first);
        cb(data, manager, ctx);
    }
}

void CacheInfo::setLoaded() {
    loading = false;
    ready = true;
    for (auto pair : callbacks) {
        pair.first(data, manager, pair.second);
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
    CacheKey key = cache.begin()->first;
    unsigned int shortest =cache.begin()->second->getTime();
    for (auto pair : cache) {
        unsigned int t = pair.second->getTime();
        if (t < shortest) {
            key = pair.second->getKey();
            shortest = t;
        }
    }
    invalidate(key);
}

void Cache::_onCacheTimer(CacheInfo *info) {
    invalidate(info->getKey());
}

shared_ptr<CacheInfo> Cache::_get(const CacheKey &key) {
    if (!cache.count(key)) {
        std::pair<CacheKey, shared_ptr<CacheInfo>> p(key, make_shared<CacheInfo>(loop, this, key));
        cache.insert(p);
    }
    return cache.at(key);
}

void Cache::loading(const CacheKey &key) {
    auto c = _get(key);
    c->setLoading();
    c->stopTimer();
}

void Cache::cancel(const CacheKey &key) {
    if (cache.count(key)) {
        cache.at(key)->cancelLoading();
    }
}

void Cache::add(const CacheKey &key, CachedData data) {
    LOG_DEBUG("Adding " << key.name);
    if (max_size > 0) {
        while (size + data.body.length() > max_size) {
            _remove_old_cache();
        }
    }
    auto c = _get(key);
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

void Cache::invalidate(const CacheKey &key) {
    LOG_INFO("invalidating cache \"" << key.name << "\"");
    if (cache.count(key)) {
        if (cache.at(key)->isLoaded()) {
            size -= cache.at(key)->getSize();
        }
        cache.erase(key);
    }
}

bool Cache::getNotified(const CacheKey &key, CacheReadyCB callback, void *ctx) {
    if (!cache.count(key)) {
        return false;
    }
    auto c = cache.at(key);
    if (c->isLoaded()) {
        // If the data is already loaded, just call the callback immediately
        callback(c->getData(), this, ctx);
        return true;
    }
    c->addCallback(callback, ctx);
    return true;
}

bool Cache::isLoading(const CacheKey &key) const {
    if (cache.count(key)) {
        return cache.at(key)->isLoading();
    }
    return false;
}

bool Cache::isLoaded(const CacheKey &key) const {
    if (cache.count(key)) {
        return cache.at(key)->isLoaded();
    }
    return false;
}

const CachedData &Cache::get(const CacheKey &key) const {
    return cache.at(key)->getData();
}