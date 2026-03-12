#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

// A minimal std::vector-like dynamic array.
// - Contiguous storage
// - Growth by reallocation
// - Move-aware, exception-safe (basic guarantee)
template <typename T>
class Vector {
public:
    using value_type = T;
    using size_type = std::size_t;

    Vector() noexcept : data_(nullptr), size_(0), capacity_(0) {}

    explicit Vector(size_type n) : Vector() {
        resize(n);
    }

    Vector(std::initializer_list<T> init) : Vector() {
        reserve(init.size());
        for (const auto& x : init) push_back(x);
    }

    Vector(const Vector& other) : Vector() {
        reserve(other.size_);
        for (size_type i = 0; i < other.size_; ++i) {
            std::allocator_traits<Alloc>::construct(alloc_, data_ + i, other.data_[i]);
        }
        size_ = other.size_;
    }

    Vector& operator=(const Vector& other) {
        if (this == &other) return *this;
        Vector tmp(other);
        swap(tmp);
        return *this;
    }

    Vector(Vector&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this == &other) return *this;
        clear();
        deallocate();
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        return *this;
    }

    ~Vector() {
        clear();
        deallocate();
    }

    void push_back(const T& value) {
        ensure_capacity_for_one_more();
        std::allocator_traits<Alloc>::construct(alloc_, data_ + size_, value);
        ++size_;
    }

    void push_back(T&& value) {
        ensure_capacity_for_one_more();
        std::allocator_traits<Alloc>::construct(alloc_, data_ + size_, std::move(value));
        ++size_;
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        ensure_capacity_for_one_more();
        std::allocator_traits<Alloc>::construct(alloc_, data_ + size_, std::forward<Args>(args)...);
        ++size_;
        return data_[size_ - 1];
    }

    void pop_back() {
        if (size_ == 0) throw std::out_of_range("Vector::pop_back on empty");
        std::allocator_traits<Alloc>::destroy(alloc_, data_ + (size_ - 1));
        --size_;
    }

    void reserve(size_type newCap) {
        if (newCap <= capacity_) return;
        reallocate(newCap);
    }

    void resize(size_type n) {
        if (n < size_) {
            while (size_ > n) pop_back();
            return;
        }
        reserve(n);
        while (size_ < n) {
            std::allocator_traits<Alloc>::construct(alloc_, data_ + size_);
            ++size_;
        }
    }

    void clear() noexcept {
        for (size_type i = 0; i < size_; ++i) {
            std::allocator_traits<Alloc>::destroy(alloc_, data_ + i);
        }
        size_ = 0;
    }

    T& operator[](size_type i) noexcept { return data_[i]; }
    const T& operator[](size_type i) const noexcept { return data_[i]; }

    T& at(size_type i) {
        if (i >= size_) throw std::out_of_range("Vector::at");
        return data_[i];
    }
    const T& at(size_type i) const {
        if (i >= size_) throw std::out_of_range("Vector::at");
        return data_[i];
    }

    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }

    size_type size() const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }

    void swap(Vector& other) noexcept {
        using std::swap;
        swap(data_, other.data_);
        swap(size_, other.size_);
        swap(capacity_, other.capacity_);
    }

    T* begin() noexcept { return data_; }
    T* end() noexcept { return data_ + size_; }
    const T* begin() const noexcept { return data_; }
    const T* end() const noexcept { return data_ + size_; }

private:
    using Alloc = std::allocator<T>;

    void ensure_capacity_for_one_more() {
        if (size_ < capacity_) return;
        const size_type newCap = (capacity_ == 0) ? 8 : capacity_ * 2;
        reallocate(newCap);
    }

    void reallocate(size_type newCap) {
        T* newData = std::allocator_traits<Alloc>::allocate(alloc_, newCap);
        size_type i = 0;
        try {
            for (; i < size_; ++i) {
                std::allocator_traits<Alloc>::construct(
                    alloc_, newData + i, std::move_if_noexcept(data_[i]));
            }
        } catch (...) {
            for (size_type j = 0; j < i; ++j) {
                std::allocator_traits<Alloc>::destroy(alloc_, newData + j);
            }
            std::allocator_traits<Alloc>::deallocate(alloc_, newData, newCap);
            throw;
        }

        clear();
        deallocate();
        data_ = newData;
        capacity_ = newCap;
        size_ = i;
    }

    void deallocate() noexcept {
        if (!data_) return;
        std::allocator_traits<Alloc>::deallocate(alloc_, data_, capacity_);
        data_ = nullptr;
        capacity_ = 0;
    }

    Alloc alloc_{};
    T* data_;
    size_type size_;
    size_type capacity_;
};

