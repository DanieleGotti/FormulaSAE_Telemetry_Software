#pragma once
#include <fstream>
#include <vector>
#include <string>
#include "IAggregatedDataSubscriber.hpp" 

class CsvWriter : public IAggregatedDataSubscriber {
public:
    CsvWriter();
    ~CsvWriter() override;
    
    CsvWriter(const CsvWriter&) = delete;
    CsvWriter& operator=(const CsvWriter&) = delete;

    bool createFile(const std::string& directoryPath, const std::vector<std::string>& columnOrder);
    void stop();
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    void closeFile();
    std::ofstream m_outputFile;
    std::vector<std::string> m_columnOrder;
};