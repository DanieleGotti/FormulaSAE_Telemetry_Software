#include <algorithm>
#include "Telemetry/Services/DataAggregatorService.hpp"
#include "Telemetry/data_writing/DataAggregator.hpp"

std::vector<ColumnConfig> DataAggregatorService::getDefaultConfig() {
    std::vector<ColumnConfig> config = {
        // Sensori che vogliamo vedere come interi
        {"ACC1A", AggregationType::AVERAGE, OutputFormat::INTEGER},
        {"ACC2A", AggregationType::AVERAGE, OutputFormat::INTEGER},
        {"ACC1B", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"ACC2B", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"BRK1",  AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"BRK2",  AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"R2D_BUTTON", AggregationType::LAST, OutputFormat::INTEGER},
        {"RESET_BUTTON", AggregationType::LAST, OutputFormat::INTEGER},
        {"SDC_INPUT", AggregationType::LAST, OutputFormat::INTEGER},
        {"TS_ON_BUTTON", AggregationType::LAST, OutputFormat::INTEGER},

        // Sensori che vogliamo vedere come decimali
        {"STEER", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"SOSPADX", AggregationType::AVERAGE, OutputFormat::DOUBLE}, 
        {"SOSPASX", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"SOSPPDX", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"SOSPPSX", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"VELADX", AggregationType::AVERAGE, OutputFormat::DOUBLE}, 
        {"VELASX", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"VELPDX", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"VELPSX", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"TMPDX", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"TMPSX", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"TMPMOTORDX", AggregationType::AVERAGE, OutputFormat::DOUBLE},
        {"TMPMOTORSX", AggregationType::AVERAGE, OutputFormat::DOUBLE},

        // Dati che sono stringhe
        {"LEFT_INVERTER_FSM",  AggregationType::INVERTER, OutputFormat::STRING},
        {"RIGHT_INVERTER_FSM", AggregationType::INVERTER, OutputFormat::STRING}
    };

    for (int i = 1; i <= 14; ++i) {
        std::string suffix = std::to_string(i);
        config.push_back({ "TMP1M" + suffix, AggregationType::AVERAGE, OutputFormat::DOUBLE });
        config.push_back({ "TMP2M" + suffix, AggregationType::AVERAGE, OutputFormat::DOUBLE });
        config.push_back({ "TENSM" + suffix, AggregationType::AVERAGE, OutputFormat::DOUBLE });
        config.push_back({ "ERRORM" + suffix, AggregationType::LAST, OutputFormat::INTEGER });
    }
    return config;
}

DataAggregatorService::DataAggregatorService() {
    m_config = getDefaultConfig();
    m_aggregator = std::make_unique<DataAggregator>(m_config, 
        [this](const DbRow& row){ this->onRowReady(row); }
    );
}

bool DataAggregatorService::createFile(const std::string& directoryPath) { 
    return true; 
}

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
    
    const std::vector<std::string> targetSensors = {
        "ACC1A", "ACC2A", "ACC1B", "ACC2B", "BRK1", "BRK2", "STEER"
    };
    for (const auto& sensor : targetSensors) {
        order.push_back(sensor + "_MEAN");
        order.push_back(sensor + "_VAR");
        order.push_back(sensor + "_STD");
    }
    
    return order;
}