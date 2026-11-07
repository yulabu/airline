#pragma once

#include <new>

#include "base/status.h"

namespace airline {

// 链队列：先进先出，入队出队 O(1)。
// 候补队列用它，因为不确定长度且频繁删除队首。
template <class T>
class LinkQueue {
public:
    LinkQueue() = default;

    ~LinkQueue() { clear(); }

    LinkQueue(const LinkQueue& other) { copy_from(other); }

    LinkQueue& operator=(const LinkQueue& other) {
        if (this != &other) {
            clear();
            copy_from(other);
        }
        return *this;
    }

    LinkQueue(LinkQueue&& other) noexcept
        : head_(other.head_), tail_(other.tail_), size_(other.size_) {
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.size_ = 0;
    }

    LinkQueue& operator=(LinkQueue&& other) noexcept {
        if (this != &other) {
            clear();
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;
            other.head_ = nullptr;
            other.tail_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    int size() const { return size_; }
    bool empty() const { return size_ == 0; }

    void clear() {
        Node* p = head_;
        while (p) {
            Node* next = p->next;
            delete p;
            p = next;
        }
        head_ = tail_ = nullptr;
        size_ = 0;
    }

    Status enqueue(const T& v) {
        Node* node = new (std::nothrow) Node{v, nullptr};
        if (!node) return Status::INTERNAL;
        if (tail_) {
            tail_->next = node;
        } else {
            head_ = node;
        }
        tail_ = node;
        ++size_;
        return Status::OK;
    }

    Status dequeue(T* out) {
        if (empty()) return Status::WAITLIST_EMPTY;
        Node* p = head_;
        if (out) *out = p->data;
        head_ = p->next;
        if (!head_) tail_ = nullptr;
        delete p;
        --size_;
        return Status::OK;
    }

    T* front() { return head_ ? &head_->data : nullptr; }
    const T* front() const { return head_ ? &head_->data : nullptr; }

    // 按位置删除，供候补撮合时"跳过队首、取走第 k 个"使用
    Status remove_at(int index, T* out) {
        if (index < 0 || index >= size_) return Status::INVALID_ARG;

        Node* prev = nullptr;
        Node* cur = head_;
        for (int i = 0; i < index; ++i) {
            prev = cur;
            cur = cur->next;
        }

        if (out) *out = cur->data;
        if (prev) {
            prev->next = cur->next;
        } else {
            head_ = cur->next;
        }
        if (cur == tail_) tail_ = prev;
        delete cur;
        --size_;
        return Status::OK;
    }

    // 非破坏性遍历：f 的签名为 void(const T&)
    template <class F>
    void for_each(F f) const {
        for (Node* p = head_; p; p = p->next) f(p->data);
    }

private:
    struct Node {
        T data;
        Node* next;
    };

    void copy_from(const LinkQueue& other) {
        for (Node* p = other.head_; p; p = p->next) {
            enqueue(p->data);
        }
    }

    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    int size_ = 0;
};

}  // namespace airline
