#pragma once
#include <atomic>
#include <vector>
#include <optional>
#include <cassert>

namespace pilot::engine {

template<typename T, std::size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
public:
    RingBuffer() : head_(0), tail_(0), buffer_(Capacity) {}

    // Producer side - returns false if full
    bool push(T item) {
        const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next_tail    = (current_tail + 1) & (Capacity - 1);
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // full
        }
        buffer_[current_tail] = std::move(item);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // Consumer side - returns nullopt if empty
    std::optional<T> pop() {
        const std::size_t current_head = head_.load(std::memory_order_relaxed);
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt; // empty
        }
        T item = std::move(buffer_[current_head]);
        head_.store((current_head + 1) & (Capacity - 1), std::memory_order_release);
        return item;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    std::size_t size() const {
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        return (tail - head + Capacity) & (Capacity - 1);
    }

    static constexpr std::size_t capacity() { return Capacity - 1; }

private:
    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
    std::vector<T> buffer_;
};

} // namespace pilot::engine
