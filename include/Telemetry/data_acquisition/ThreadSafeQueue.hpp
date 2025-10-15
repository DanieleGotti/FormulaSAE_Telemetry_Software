#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

template <typename T>
class ThreadSafeQueue {
public:
    // Inserisce un elemento nella coda e notifica un thread in attesa
    void push(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(value));
        m_cond.notify_one();
    }
    // Attende fino a un timeout specificato per un elemento
    std::optional<T> wait_for_and_pop(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_cond.wait_for(lock, timeout, [this]{ return !m_queue.empty(); })) {
            T value = std::move(m_queue.front());
            m_queue.pop();
            return value;
        }
        return std::nullopt;
    }
    // Tenta di estrarre un elemento senza bloccarsi
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return std::nullopt;
        }
        T value = std::move(m_queue.front());
        m_queue.pop();
        return value;
    }
    // Rimuove tutti gli elementi dalla coda.
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