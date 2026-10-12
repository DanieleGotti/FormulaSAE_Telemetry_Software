/**
 * @file ThreadSafeQueue.hpp
 * @brief Thread-safe FIFO queue used to pass data between threads.
 *
 * @details
 * In SerialService it links the acquisition thread (producer: reads the serial
 * port) to the processing thread (consumer: decodes the packets), so hardware
 * reads are never slowed down by parsing.
 */
#pragma once
#include <queue>
#include <mutex>
#include <optional>
#include <chrono>
#include <condition_variable>

/**
 * @brief Mutex-protected FIFO queue with timed wait.
 * @tparam T Element type (must be movable).
 */
template <typename T>
class ThreadSafeQueue {
public:
    /**
     * @brief Adds an element and wakes up a waiting thread, if any.
     * @param value Element to add (moved into the queue).
     */
    void push(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(value));
        m_cond.notify_one();
    }
    /**
     * @brief Waits at most `timeout` for an element and pops it.
     * @param timeout Maximum waiting time.
     * @return The element, or std::nullopt if the timeout expired with an empty queue.
     */
    std::optional<T> wait_for_and_pop(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_cond.wait_for(lock, timeout, [this]{ return !m_queue.empty(); })) {
            T value = std::move(m_queue.front());
            m_queue.pop();
            return value;
        }
        return std::nullopt;
    }
    /**
     * @brief Pops an element without blocking.
     * @return The element, or std::nullopt if the queue is empty.
     */
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return std::nullopt;
        }
        T value = std::move(m_queue.front());
        m_queue.pop();
        return value;
    }
    /** @brief Empties the queue, discarding all elements. */
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::queue<T> empty;
        m_queue.swap(empty);
    }

private:
    std::queue<T> m_queue;           ///< Pending elements.
    std::mutex m_mutex;              ///< Protects m_queue.
    std::condition_variable m_cond;  ///< Signals the arrival of new elements.
};