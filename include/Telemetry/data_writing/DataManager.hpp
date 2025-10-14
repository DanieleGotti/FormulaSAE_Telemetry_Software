#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include "IWritingSubscriber.hpp"
#include "../data_acquisition/PacketParser.hpp"

class DataManager {
public:
    void addSubscriber(std::shared_ptr<IWritingSubscriber> subscriber);
    // Chiamato dal PacketParser
    void processData(const PacketParser& packet);
private:
    // Lista di subscriber (puntatori per evitare riferimenti circolari)
    std::vector<std::weak_ptr<IWritingSubscriber>> m_subscribers;
    std::mutex m_subscriberMutex;
};
