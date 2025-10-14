#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include "IWritingSubscriber.hpp"
#include "../data_acquisition/PacketParser.hpp"

class DataManager {
public:
    void addSubscriber(IWritingSubscriber* subscriber);
    // Chiamato dal PacketParser
    void processData(const ParsedPacket& packet);
private:
    std::vector<IWritingSubscriber*> subscribers;
    std::mutex m_subscriberMutex;
};
