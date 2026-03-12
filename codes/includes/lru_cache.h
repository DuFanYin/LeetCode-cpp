#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <utility>

#include "hash_table.h"

// A minimal LRU cache.
// - O(1) get/put average using list + open-addressing hash table
// - Not thread-safe
template <typename K, typename V, typename Hash = DefaultHash, typename Eq = std::equal_to<K>>
class LRUCache {
public:
    using size_type = std::size_t;

    explicit LRUCache(size_type capacity) : capacity_(capacity) {}

    size_type capacity() const noexcept { return capacity_; }
    size_type size() const noexcept { return index_.size(); }
    bool empty() const noexcept { return index_.empty(); }

    void clear() {
        index_.clear();
        lru_.clear();
    }

    std::optional<V> get(const K& key) {
        auto* it = index_.find(key);
        if (!it) return std::nullopt;
        touch(*it);
        return (*it)->second;
    }

    // Insert or update. Evicts LRU if needed.
    void put(const K& key, const V& value) {
        if (capacity_ == 0) return;
        if (auto* it = index_.find(key)) {
            (*it)->second = value;
            touch(*it);
            return;
        }
        if (index_.size() == capacity_) {
            evict_one();
        }
        lru_.emplace_front(key, value);
        index_.insert_or_assign(key, lru_.begin());
    }

    void put(K&& key, V&& value) {
        if (capacity_ == 0) return;
        if (auto* it = index_.find(key)) {
            (*it)->second = std::move(value);
            touch(*it);
            return;
        }
        if (index_.size() == capacity_) {
            evict_one();
        }
        lru_.emplace_front(std::move(key), std::move(value));
        index_.insert_or_assign(lru_.front().first, lru_.begin());
    }

    bool erase(const K& key) {
        auto* it = index_.find(key);
        if (!it) return false;
        lru_.erase(*it);
        index_.erase(key);
        return true;
    }

private:
    using Node = std::pair<K, V>;
    using ListIt = typename std::list<Node>::iterator;

    void touch(ListIt it) {
        // Move to front (most recently used).
        lru_.splice(lru_.begin(), lru_, it);
    }

    void evict_one() {
        auto last = std::prev(lru_.end());
        index_.erase(last->first);
        lru_.pop_back();
    }

    size_type capacity_;
    std::list<Node> lru_;
    HashTable<K, ListIt, Hash, Eq> index_;
};

