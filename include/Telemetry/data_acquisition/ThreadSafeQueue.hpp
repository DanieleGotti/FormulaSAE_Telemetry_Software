#pragma once
#include <queue>
#include <mutex>
#include <optional>
#include <atomic>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue {
public:

    ThreadSafeQueue() : m_running(true) {} 

    // Aggiunge un elemento alla coda
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(value));
        }
        m_cond.notify_one();
    }

    // Attende finchè un elemento è disponibile o la coda è fermata, poi lo estrae
    std::optional<T> wait_and_pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this]{ return !m_queue.empty(); });
        
        if (!m_running && m_queue.empty()) {
            return std::nullopt; 
        }

        T value = std::move(m_queue.front());
        m_queue.pop();        
        
        return value;
    }

    // Ferma la coda, svegliando eventuali thread in attesa
    void stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_running = false;
        }
        m_cond.notify_all();
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::atomic<bool> m_running;
};