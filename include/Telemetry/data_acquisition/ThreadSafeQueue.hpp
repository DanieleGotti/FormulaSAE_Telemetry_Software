#pragma once
#include <queue>
#include <mutex>
#include <optional>
#include <chrono>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue {
public:
    // Add an element to the queue and notify a waiting thread
    void push(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(value));
        m_cond.notify_one();
    }
    // Wait for a specified timeout period for an element
    std::optional<T> wait_for_and_pop(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_cond.wait_for(lock, timeout, [this]{ return !m_queue.empty(); })) {
            T value = std::move(m_queue.front());
            m_queue.pop();
            return value;
        }
        return std::nullopt;
    }
    // Attempt to extract an element without blocking
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return std::nullopt;
        }
        T value = std::move(m_queue.front());
        m_queue.pop();
        return value;
    }
    // Remove all elements from the queue
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::queue<T> empty;
        m_queue.swap(empty);
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};