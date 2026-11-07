#pragma once

#include <new>

#include "base/status.h"

namespace airline {

// 顺序表：连续存储，随机访问 O(1)，中间插入/删除 O(n)。
// 航线表用它，因为查询远多于增删。
template <class T>
class SeqList {
public:
    SeqList() = default;

    ~SeqList() { delete[] buf_; }

    SeqList(const SeqList& other) { copy_from(other); }

    SeqList& operator=(const SeqList& other) {
        if (this != &other) {
            delete[] buf_;
            buf_ = nullptr;
            size_ = 0;
            cap_ = 0;
            copy_from(other);
        }
        return *this;
    }

    SeqList(SeqList&& other) noexcept
        : buf_(other.buf_), size_(other.size_), cap_(other.cap_) {
        other.buf_ = nullptr;
        other.size_ = 0;
        other.cap_ = 0;
    }

    SeqList& operator=(SeqList&& other) noexcept {
        if (this != &other) {
            delete[] buf_;
            buf_ = other.buf_;
            size_ = other.size_;
            cap_ = other.cap_;
            other.buf_ = nullptr;
            other.size_ = 0;
            other.cap_ = 0;
        }
        return *this;
    }

    int size() const { return size_; }
    int capacity() const { return cap_; }
    bool empty() const { return size_ == 0; }

    // 只清空元素，不释放缓冲区
    void clear() { size_ = 0; }

    T& at(int i) { return buf_[i]; }
    const T& at(int i) const { return buf_[i]; }

    T& operator[](int i) { return buf_[i]; }
    const T& operator[](int i) const { return buf_[i]; }

    T* data() { return buf_; }
    const T* data() const { return buf_; }

    Status push_back(const T& v) {
        if (size_ == cap_ && !grow(cap_ ? cap_ * 2 : kInitCap)) {
            return Status::INTERNAL;
        }
        buf_[size_++] = v;
        return Status::OK;
    }

    Status insert_at(int i, const T& v) {
        if (i < 0 || i > size_) return Status::INVALID_ARG;
        if (size_ == cap_ && !grow(cap_ ? cap_ * 2 : kInitCap)) {
            return Status::INTERNAL;
        }
        for (int k = size_; k > i; --k) buf_[k] = buf_[k - 1];
        buf_[i] = v;
        ++size_;
        return Status::OK;
    }

    Status remove_at(int i) {
        if (i < 0 || i >= size_) return Status::INVALID_ARG;
        for (int k = i; k + 1 < size_; ++k) buf_[k] = buf_[k + 1];
        --size_;
        return Status::OK;
    }

    // 需要 T 支持 operator==；找不到返回 -1
    int find_index(const T& v) const {
        for (int i = 0; i < size_; ++i) {
            if (buf_[i] == v) return i;
        }
        return -1;
    }

private:
    bool grow(int new_cap) {
        T* nb = new (std::nothrow) T[new_cap];
        if (!nb) return false;
        for (int i = 0; i < size_; ++i) nb[i] = buf_[i];
        delete[] buf_;
        buf_ = nb;
        cap_ = new_cap;
        return true;
    }

    void copy_from(const SeqList& other) {
        if (other.size_ == 0) return;
        buf_ = new T[other.cap_];
        cap_ = other.cap_;
        for (int i = 0; i < other.size_; ++i) buf_[i] = other.buf_[i];
        size_ = other.size_;
    }

    static const int kInitCap = 8;

    T* buf_ = nullptr;
    int size_ = 0;
    int cap_ = 0;
};

}  // namespace airline
