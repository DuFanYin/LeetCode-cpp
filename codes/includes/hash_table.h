#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Default hash that avoids libc++'s std::hash<std::string> dependency.
// (Some toolchains/configs may fail to link std::__1::__hash_memory.)
struct DefaultHash {
    using is_transparent = void;

    static std::size_t fnv1a_bytes(const unsigned char* p, std::size_t n) noexcept {
        std::size_t h = 1469598103934665603ull;
        for (std::size_t i = 0; i < n; ++i) {
            h ^= static_cast<std::size_t>(p[i]);
            h *= 1099511628211ull;
        }
        return h;
    }

    std::size_t operator()(std::string_view s) const noexcept {
        return fnv1a_bytes(reinterpret_cast<const unsigned char*>(s.data()), s.size());
    }

    std::size_t operator()(const std::string& s) const noexcept {
        return (*this)(std::string_view{s});
    }

    std::size_t operator()(const char* s) const noexcept {
        if (!s) return 0;
        return (*this)(std::string_view{s});
    }

    template <typename T>
    std::size_t operator()(const T& x) const noexcept {
        if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
            return static_cast<std::size_t>(x) * 11400714819323198485ull;
        } else if constexpr (std::is_pointer_v<T>) {
            return (reinterpret_cast<std::size_t>(x) >> 4) * 11400714819323198485ull;
        } else {
            // Fallback: hash raw bytes of the object representation.
            // Only intended for demos and types with stable byte representation.
            return fnv1a_bytes(reinterpret_cast<const unsigned char*>(&x), sizeof(T));
        }
    }
};

// A minimal open-addressing hash table (linear probing).
// - Supports insert/update, erase, find.
// - Not optimized; intended for learning and small demos.
template <typename K, typename V, typename Hash = DefaultHash, typename Eq = std::equal_to<K>>
class HashTable {
public:
    using size_type = std::size_t;

    HashTable() : buckets_(initial_capacity()) {}

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }

    void clear() {
        buckets_.assign(buckets_.size(), Bucket{});
        size_ = 0;
        tombstones_ = 0;
    }

    // Insert or assign. Returns true if inserted new key.
    bool insert_or_assign(const K& key, const V& value) { return emplace_or_assign(key, value); }
    bool insert_or_assign(K&& key, V&& value) { return emplace_or_assign(std::move(key), std::move(value)); }

    bool erase(const K& key) {
        const auto idx = find_index(key);
        if (!idx) return false;
        Bucket& b = buckets_[*idx];
        b.state = State::Tombstone;
        b.k.reset();
        b.v.reset();
        --size_;
        ++tombstones_;
        maybe_rehash();
        return true;
    }

    V* find(const K& key) {
        const auto idx = find_index(key);
        if (!idx) return nullptr;
        return &(*buckets_[*idx].v);
    }

    const V* find(const K& key) const {
        const auto idx = find_index(key);
        if (!idx) return nullptr;
        return &(*buckets_[*idx].v);
    }

    V& at(const K& key) {
        auto* p = find(key);
        if (!p) throw std::out_of_range("HashTable::at");
        return *p;
    }

private:
    enum class State : std::uint8_t { Empty, Filled, Tombstone };

    struct Bucket {
        State state = State::Empty;
        std::optional<K> k;
        std::optional<V> v;
    };

    static constexpr size_type initial_capacity() { return 16; }

    double load_factor() const noexcept {
        return buckets_.empty() ? 0.0 : static_cast<double>(size_ + tombstones_) / static_cast<double>(buckets_.size());
    }

    void maybe_rehash() {
        if (load_factor() > 0.7) {
            rehash(buckets_.size() * 2);
        } else if (tombstones_ > size_ && buckets_.size() > initial_capacity()) {
            // Too many tombstones: rebuild at same capacity.
            rehash(buckets_.size());
        }
    }

    void rehash(size_type newCap) {
        std::vector<Bucket> old = std::move(buckets_);
        buckets_.assign(newCap, Bucket{});
        size_ = 0;
        tombstones_ = 0;
        for (auto& b : old) {
            if (b.state == State::Filled) {
                emplace_or_assign(std::move(*b.k), std::move(*b.v));
            }
        }
    }

    template <typename KK, typename VV>
    bool emplace_or_assign(KK&& key, VV&& value) {
        maybe_rehash();
        auto [idx, found] = find_slot(key);
        Bucket& b = buckets_[idx];
        if (found) {
            *b.v = std::forward<VV>(value);
            return false;
        }
        if (b.state == State::Tombstone) {
            --tombstones_;
        }
        b.state = State::Filled;
        b.k = std::forward<KK>(key);
        b.v = std::forward<VV>(value);
        ++size_;
        return true;
    }

    std::pair<size_type, bool> find_slot(const K& key) const {
        size_type cap = buckets_.size();
        size_type start = hasher_(key) & (cap - 1);
        std::optional<size_type> firstTomb;
        for (size_type i = 0; i < cap; ++i) {
            size_type idx = (start + i) & (cap - 1);
            const Bucket& b = buckets_[idx];
            if (b.state == State::Empty) {
                return {firstTomb ? *firstTomb : idx, false};
            }
            if (b.state == State::Tombstone) {
                if (!firstTomb) firstTomb = idx;
                continue;
            }
            if (eq_(*b.k, key)) {
                return {idx, true};
            }
        }
        // Shouldn't happen if rehashing keeps load factor under control.
        return {firstTomb.value_or(0), false};
    }

    std::optional<size_type> find_index(const K& key) const {
        size_type cap = buckets_.size();
        size_type start = hasher_(key) & (cap - 1);
        for (size_type i = 0; i < cap; ++i) {
            size_type idx = (start + i) & (cap - 1);
            const Bucket& b = buckets_[idx];
            if (b.state == State::Empty) return std::nullopt;
            if (b.state == State::Filled && eq_(*b.k, key)) return idx;
        }
        return std::nullopt;
    }

    static_assert((initial_capacity() & (initial_capacity() - 1)) == 0, "capacity must be power of two");

    Hash hasher_{};
    Eq eq_{};
    std::vector<Bucket> buckets_;
    size_type size_ = 0;
    size_type tombstones_ = 0;
};

