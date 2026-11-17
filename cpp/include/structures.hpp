#pragma once

#include <cstddef>
#include <stdexcept>

// Arreglo de tamano fijo con propiedad exclusiva de su memoria.
template <typename T>
class Array {
    T* data_;
    std::size_t size_;
public:
    explicit Array(std::size_t size = 0) : data_(size ? new T[size]{} : nullptr), size_(size) {}
    ~Array() { delete[] data_; }
    Array(const Array&) = delete;
    Array& operator=(const Array&) = delete;
    Array(Array&& other) noexcept : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }
    Array& operator=(Array&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
    T& operator[](std::size_t index) { return data_[index]; }
    const T& operator[](std::size_t index) const { return data_[index]; }
    T* data() { return data_; }
    std::size_t size() const { return size_; }
};

// Cola circular FIFO. Cada nodo se encola como maximo una vez por BFS.
class Queue {
    Array<int> data_;
    std::size_t head_ = 0, tail_ = 0, count_ = 0;
public:
    explicit Queue(std::size_t capacity) : data_(capacity) {}
    bool empty() const { return count_ == 0; }
    void enqueue(int node) {
        if (count_ == data_.size()) throw std::runtime_error("Cola llena: invariante interna rota.");
        data_[tail_] = node;
        tail_ = (tail_ + 1) % data_.size();
        ++count_;
    }
    int dequeue() {
        if (empty()) throw std::runtime_error("Cola vacia: invariante interna rota.");
        int node = data_[head_];
        head_ = (head_ + 1) % data_.size();
        --count_;
        return node;
    }
};
