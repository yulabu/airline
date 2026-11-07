#pragma once

#include <cstddef>
#include <string>

#include "base/status.h"

namespace airline {

// 默认哈希函数。自定义键类型需要自行特化，或作为第三个模板参数传入。
template <class K>
struct DefaultHash;

template <>
struct DefaultHash<std::string> {
    std::size_t operator()(const std::string& s) const {
        // FNV-1a 64 位
        std::size_t h = 1469598103934665603ull;
        for (unsigned char c : s) {
            h ^= static_cast<std::size_t>(c);
            h *= 1099511628211ull;
        }
        return h;
    }
};

template <>
struct DefaultHash<int> {
    std::size_t operator()(int v) const {
        // Knuth 乘法散列，避免小整数直接落桶导致聚集
        return static_cast<std::size_t>(v) * 2654435761u;
    }
};

// 哈希索引：拉链法处理冲突，平均查找 O(1)。
// 用于"航班号 → 航线"和"(航班号, 日期) → 航班实例"两级索引。
template <class K, class V, class H = DefaultHash<K>>
class HashIndex {
public:
    explicit HashIndex(int bucket_count = 64) {
        if (bucket_count < 8) bucket_count = 8;
        buckets_ = new Entry*[bucket_count]();
        bucket_count_ = bucket_count;
        count_ = 0;
    }

    ~HashIndex() { destroy(); }

    HashIndex(const HashIndex&) = delete;
    HashIndex& operator=(const HashIndex&) = delete;

    int size() const { return count_; }
    int bucket_count() const { return bucket_count_; }

    Status put(const K& key, const V& value) {
        std::size_t idx = H{}(key) & static_cast<std::size_t>(bucket_count_ - 1);

        for (Entry* p = buckets_[idx]; p; p = p->next) {
            if (p->key == key) {
                p->value = value;
                return Status::OK;
            }
        }

        Entry* node = new Entry{key, value, buckets_[idx]};
        buckets_[idx] = node;
        ++count_;

        if (count_ > bucket_count_ * 3 / 4) rehash(bucket_count_ * 2);
        return Status::OK;
    }

    V* get(const K& key) {
        std::size_t idx = H{}(key) & static_cast<std::size_t>(bucket_count_ - 1);
        for (Entry* p = buckets_[idx]; p; p = p->next) {
            if (p->key == key) return &p->value;
        }
        return nullptr;
    }

    const V* get(const K& key) const {
        std::size_t idx = H{}(key) & static_cast<std::size_t>(bucket_count_ - 1);
        for (Entry* p = buckets_[idx]; p; p = p->next) {
            if (p->key == key) return &p->value;
        }
        return nullptr;
    }

    bool remove(const K& key) {
        std::size_t idx = H{}(key) & static_cast<std::size_t>(bucket_count_ - 1);
        Entry* prev = nullptr;
        for (Entry* p = buckets_[idx]; p; prev = p, p = p->next) {
            if (p->key == key) {
                if (prev) {
                    prev->next = p->next;
                } else {
                    buckets_[idx] = p->next;
                }
                delete p;
                --count_;
                return true;
            }
        }
        return false;
    }

    void clear() {
        std::size_t n = static_cast<std::size_t>(bucket_count_);
        for (std::size_t i = 0; i < n; ++i) buckets_[i] = nullptr;
        destroy_nodes();
        count_ = 0;
    }

    // 非破坏性遍历：f 的签名为 void(const K&, V&)
    template <class F>
    void for_each(F f) {
        for (int i = 0; i < bucket_count_; ++i) {
            for (Entry* p = buckets_[i]; p; p = p->next) f(p->key, p->value);
        }
    }

    // 报告用：最长链长度，衡量哈希函数质量
    int max_bucket_length() const {
        int worst = 0;
        for (int i = 0; i < bucket_count_; ++i) {
            int len = 0;
            for (Entry* p = buckets_[i]; p; p = p->next) ++len;
            if (len > worst) worst = len;
        }
        return worst;
    }

private:
    struct Entry {
        K key;
        V value;
        Entry* next;
    };

    void destroy_nodes() {
        for (int i = 0; i < bucket_count_; ++i) {
            Entry* p = buckets_[i];
            while (p) {
                Entry* next = p->next;
                delete p;
                p = next;
            }
            buckets_[i] = nullptr;
        }
    }

    void destroy() {
        if (!buckets_) return;
        destroy_nodes();
        delete[] buckets_;
        buckets_ = nullptr;
        bucket_count_ = 0;
        count_ = 0;
    }

    void rehash(int new_bucket_count) {
        Entry** nb = new Entry*[new_bucket_count]();
        for (int i = 0; i < bucket_count_; ++i) {
            Entry* p = buckets_[i];
            while (p) {
                Entry* next = p->next;
                std::size_t idx = H{}(p->key) & static_cast<std::size_t>(new_bucket_count - 1);
                p->next = nb[idx];
                nb[idx] = p;
                p = next;
            }
        }
        delete[] buckets_;
        buckets_ = nb;
        bucket_count_ = new_bucket_count;
    }

    Entry** buckets_ = nullptr;
    int bucket_count_ = 0;
    int count_ = 0;
};

}  // namespace airline
