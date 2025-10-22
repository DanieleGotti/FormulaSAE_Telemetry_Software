#include <algorithm>
#include "Telemetry/Services/DataAggregatorService.hpp"

DataAggregatorService::DataAggregatorService() {
    // Per aggiungere/modificare una colonna al database
    m_config = {
        {"ACC1A", AggregationType::AVERAGE}, {"ACC1B", AggregationType::AVERAGE},
        {"ACC2A", AggregationType::AVERAGE}, {"ACC2B", AggregationType::AVERAGE},
        {"BRK1", AggregationType::AVERAGE}, {"BRK2", AggregationType::AVERAGE},
        {"STEER", AggregationType::AVERAGE},
        {"LEFT_INVERTER_FSM", AggregationType::INVERTER},
        {"RIGHT_INVERTER_FSM", AggregationType::INVERTER},
        {"R2D_BUTTON", AggregationType::LAST}, {"RESET_BUTTON", AggregationType::LAST},
        {"SDC_INPUT", AggregationType::LAST}, {"TS_ON_BUTTON", AggregationType::LAST}
    };

    m_aggregator = std::make_unique<DataAggregator>(m_config, 
        [this](const DbRow& row){ this->onRowReady(row); }
    );
}

// Non è usato 
bool DataAggregatorService::createFile(const std::string& directoryPath) { return true; }

void DataAggregatorService::onDataReceived(const PacketParser& packet) {
    if (m_aggregator) {
        m_aggregator->processPacket(packet);
    }
}

void DataAggregatorService::flush() {
    if (m_aggregator) {
        m_aggregator->flush();
    }
}

void DataAggregatorService::subscribe(IAggregatedDataSubscriber* subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_subscribers.push_back(subscriber);
}

void DataAggregatorService::unsubscribe(IAggregatedDataSubscriber* subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_subscribers.erase(
        std::remove(m_subscribers.begin(), m_subscribers.end(), subscriber),
        m_subscribers.end()
    );
}

void DataAggregatorService::onRowReady(const DbRow& row) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    for (IAggregatedDataSubscriber* sub : m_subscribers) {
        sub->onAggregatedDataReceived(row);
    }
}

std::vector<std::string> DataAggregatorService::getColumnOrder() const {
    std::vector<std::string> order;
    order.push_back("timestamp");
    for(const auto& col : m_config) {
        order.push_back(col.name);
    }
    return order;
}