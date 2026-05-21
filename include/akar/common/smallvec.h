#pragma once
#include <cstring>
#include <cstdint>
#include <algorithm>

namespace akar {

// SmallVec: inline-buffer optimized small vector
// Stores up to N elements inline (no heap allocation).
// Falls back to heap for larger counts.
// Designed for signal subscribers (typically 1-3) and effect dependencies (typically 1-5).
template<typename T, int N = 4>
class SmallVec {
public:
    SmallVec() = default;
    
    ~SmallVec() {
        if (data_ != inline_) delete[] data_;
    }
    
    // Copy
    SmallVec(const SmallVec& other) : size_(other.size_), capacity_(other.capacity_) {
        if (other.data_ == other.inline_) {
            data_ = inline_;
            std::memcpy(inline_, other.inline_, size_ * sizeof(T));
        } else {
            data_ = new T[capacity_];
            std::memcpy(data_, other.data_, size_ * sizeof(T));
        }
    }
    
    SmallVec& operator=(const SmallVec& other) {
        if (this == &other) return *this;
        if (data_ != inline_) delete[] data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        if (other.data_ == other.inline_) {
            data_ = inline_;
            std::memcpy(inline_, other.inline_, size_ * sizeof(T));
        } else {
            data_ = new T[capacity_];
            std::memcpy(data_, other.data_, size_ * sizeof(T));
        }
        return *this;
    }
    
    // Move
    SmallVec(SmallVec&& other) noexcept : size_(other.size_), capacity_(other.capacity_) {
        if (other.data_ == other.inline_) {
            data_ = inline_;
            std::memcpy(inline_, other.inline_, size_ * sizeof(T));
        } else {
            data_ = other.data_;
            other.data_ = other.inline_;
        }
        other.size_ = 0;
    }
    
    T& operator[](int i) { return data_[i]; }
    const T& operator[](int i) const { return data_[i]; }
    int size() const { return size_; }
    bool empty() const { return size_ == 0; }
    T* data() { return data_; }
    const T* data() const { return data_; }
    
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }
    
    void push_back(const T& val) {
        if (size_ >= capacity_) grow();
        data_[size_++] = val;
    }
    
    void clear() { size_ = 0; }
    
    // Remove first occurrence. Returns true if found.
    bool erase(const T& val) {
        for (int i = 0; i < size_; i++) {
            if (data_[i] == val) {
                // Swap with last (unordered removal, O(1))
                data_[i] = data_[--size_];
                return true;
            }
        }
        return false;
    }
    
    // Check if contains value (linear scan, fast for small N)
    bool contains(const T& val) const {
        for (int i = 0; i < size_; i++) {
            if (data_[i] == val) return true;
        }
        return false;
    }
    
private:
    void grow() {
        int new_cap = capacity_ * 2;
        T* new_data = new T[new_cap];
        std::memcpy(new_data, data_, size_ * sizeof(T));
        if (data_ != inline_) delete[] data_;
        data_ = new_data;
        capacity_ = new_cap;
    }
    
    T inline_[N];
    T* data_ = inline_;
    int size_ = 0;
    int capacity_ = N;
};

} // namespace akar
