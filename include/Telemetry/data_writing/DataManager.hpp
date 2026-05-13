#pragma once
#include <vector>
#include <mutex>
#include "IAggregatedDataSubscriber.hpp" 

class DataManager {
public:
    // Subscription for the UI (limited to 10 Hz)
    void addSubscriber(IAggregatedDataSubscriber* subscriber);
    void removeSubscriber(IAggregatedDataSubscriber* subscriber);
    
    // Subscriptions for Logs/Datasets (maximum speed 200 Hz)
    void addLogSubscriber(IAggregatedDataSubscriber* subscriber);
    void removeLogSubscriber(IAggregatedDataSubscriber* subscriber);
    
    void processData(const DbRow& row);

private:
    std::vector<IAggregatedDataSubscriber*> m_subscribers;
    std::vector<IAggregatedDataSubscriber*> m_logSubscribers;
    std::mutex m_subscriberMutex;
    
    // Timer to limit the UI update rate
    std::chrono::steady_clock::time_point m_lastUiUpdate;
};