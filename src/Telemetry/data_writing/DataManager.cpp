#include <algorithm>
#include "Telemetry/data_writing/DataManager.hpp"

void DataManager::addSubscriber(IWritingSubscriber* subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_subscribers.push_back(subscriber); 
}

void DataManager::removeSubscriber(IWritingSubscriber* subscriberToRemove) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_subscribers.erase(
        std::remove(m_subscribers.begin(), m_subscribers.end(), subscriberToRemove),
        m_subscribers.end()
    );
}

void DataManager::processData(const PacketParser& packet) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    
    // Ora il ciclo è molto più semplice
    for (IWritingSubscriber* subscriber : m_subscribers) {
        if (subscriber) {
            subscriber->onDataReceived(packet);
        }
    }
}