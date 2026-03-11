#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <map>
#include <vector>
#include <chrono>
#include "../data_writing/IWritingSubscriber.hpp"
#include "../data_writing/DataAggregator.hpp"
#include "../data_writing/IAggregatedDataSubscriber.hpp"

class DataAggregatorService : public IWritingSubscriber {
public:
    DataAggregatorService();
    
    // Metodi da IWritingSubscriber per ricevere dati grezzi
    bool createFile(const std::string& directoryPath) override;
    void onDataReceived(const PacketParser& packet) override;
    void flush(); 
    // Metodi per la gestione dei subscriber di dati aggregati
    void subscribe(IAggregatedDataSubscriber* subscriber);
    void unsubscribe(IAggregatedDataSubscriber* subscriber);
    std::vector<std::string> getColumnOrder() const;
    // Configurazione di default delle colonne
    static std::vector<ColumnConfig> getDefaultConfig();

private:
    std::unique_ptr<DataAggregator> m_aggregator;
    std::vector<IAggregatedDataSubscriber*> m_subscribers;
    std::mutex m_subscriberMutex;
    std::vector<ColumnConfig> m_config;

    void onRowReady(const DbRow& row);

};