#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include "IWritingSubscriber.hpp"
#include "../data_acquisition/PacketParser.hpp"

class DataManager {
public:
    void addSubscriber(IWritingSubscriber* subscriber);
    void removeSubscriber(IWritingSubscriber* subscriber);
    void processData(const PacketParser& packet);

private:
    std::vector<IWritingSubscriber*> m_subscribers;
    std::mutex m_subscriberMutex;
};