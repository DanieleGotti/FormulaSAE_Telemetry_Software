#pragma once
#include <vector>
#include <mutex>
#include "IAggregatedDataSubscriber.hpp" 

class DataManager {
public:
    // Sottoscrizioni per la UI (limitata a 10 Hz)
    void addSubscriber(IAggregatedDataSubscriber* subscriber);
    void removeSubscriber(IAggregatedDataSubscriber* subscriber);
    
    // Sottoscrizioni per i Log/Dataset (velocità massima 200 Hz)
    void addLogSubscriber(IAggregatedDataSubscriber* subscriber);
    void removeLogSubscriber(IAggregatedDataSubscriber* subscriber);
    
    void processData(const DbRow& row);

private:
    std::vector<IAggregatedDataSubscriber*> m_subscribers;
    std::vector<IAggregatedDataSubscriber*> m_logSubscribers;
    std::mutex m_subscriberMutex;
    
    // Timer per limitare la UI
    std::chrono::steady_clock::time_point m_lastUiUpdate;
};