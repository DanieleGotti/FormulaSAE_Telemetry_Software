#pragma once
#include <vector>
#include <mutex>
#include "IAggregatedDataSubscriber.hpp" 

class DataManager {
public:
    void addSubscriber(IAggregatedDataSubscriber* subscriber);
    void removeSubscriber(IAggregatedDataSubscriber* subscriber);
    
    // Invia la riga finale a tutti gli iscritti (UI, CsvWriter)
    void processData(const DbRow& row);

private:
    std::vector<IAggregatedDataSubscriber*> m_subscribers;
    std::mutex m_subscriberMutex;
};