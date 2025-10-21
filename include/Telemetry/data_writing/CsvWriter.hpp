#pragma once
#include <fstream>
#include <memory>
#include "IWritingSubscriber.hpp"
#include "DataAggregator.hpp"

class CsvWriter : public IWritingSubscriber {
public:
    CsvWriter();
    ~CsvWriter() override;
    
    // No copia e spostamento
    CsvWriter(const CsvWriter&) = delete;
    CsvWriter& operator=(const CsvWriter&) = delete;

    // Metodi dell'interfaccia 
    bool createFile(const std::string& directoryPath) override;
    void onDataReceived(const PacketParser& packet) override;
    void stop(); 

private:
    void onRowReady(const DbRow& row);
    void closeFile();

    std::unique_ptr<DataAggregator> m_aggregator;
    std::ofstream m_outputFile;
    std::vector<std::string> m_columnOrder;
};