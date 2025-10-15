#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include "IWritingSubscriber.hpp"
#include "../data_acquisition/PacketParser.hpp"

class DataManager {
public:
    void addSubscriber(std::shared_ptr<IWritingSubscriber> subscriber);
    void removeSubscriber(std::shared_ptr<IWritingSubscriber> subscriber);
    void processData(const PacketParser& packet);
private:
    std::vector<std::weak_ptr<IWritingSubscriber>> m_subscribers;
    std::mutex m_subscriberMutex;
};
