#include "Telemetry/data_writing/DataManager.hpp"
#include <algorithm>

void DataManager::addSubscriber(IAggregatedDataSubscriber* subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_subscribers.push_back(subscriber);
}

void DataManager::removeSubscriber(IAggregatedDataSubscriber* subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_subscribers.erase(
        std::remove(m_subscribers.begin(), m_subscribers.end(), subscriber),
        m_subscribers.end()
    );
}

void DataManager::processData(const DbRow& row) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    for (IAggregatedDataSubscriber* sub : m_subscribers) {
        if (sub) {
            sub->onAggregatedDataReceived(row);
        }
    }
}