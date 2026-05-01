#include "Telemetry/data_writing/DataManager.hpp"
#include <algorithm>

void DataManager::addSubscriber(IAggregatedDataSubscriber* subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_subscribers.push_back(subscriber);
}

void DataManager::removeSubscriber(IAggregatedDataSubscriber* subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_subscribers.erase(std::remove(m_subscribers.begin(), m_subscribers.end(), subscriber), m_subscribers.end());
}

void DataManager::addLogSubscriber(IAggregatedDataSubscriber* subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_logSubscribers.push_back(subscriber);
}

void DataManager::removeLogSubscriber(IAggregatedDataSubscriber* subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_logSubscribers.erase(std::remove(m_logSubscribers.begin(), m_logSubscribers.end(), subscriber), m_logSubscribers.end());
}

void DataManager::processData(const DbRow& row) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    
    // 1. Invia immediatamente i dati ai loggers (Dataset/CSV) senza limiti di velocità
    for (IAggregatedDataSubscriber* sub : m_logSubscribers) {
        if (sub) sub->onAggregatedDataReceived(row);
    }

    // 2. Limita l'aggiornamento della UI a 1 decimo di secondo (10 Hz)
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUiUpdate).count() >= 100) {
        for (IAggregatedDataSubscriber* sub : m_subscribers) {
            if (sub) sub->onAggregatedDataReceived(row);
        }
        m_lastUiUpdate = now;
    }
}