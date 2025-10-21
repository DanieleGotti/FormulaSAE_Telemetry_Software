#pragma once
#include <mutex>
#include <map>
#include <string>
#include <functional> 
#include "UI/UIElement.hpp"
#include "Telemetry/data_writing/IWritingSubscriber.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"

class Dashboard : public UIElement, public IWritingSubscriber {
public:
    Dashboard(); 
    void draw() override;
    void onDataReceived(const PacketParser& packet) override;
    bool createFile(const std::string& directoryPath) override;
    
private:
    const std::string OUTPUT_DIRECTORY = "../output_data"; 
    std::map<std::string, PacketData> m_latestData;
    std::mutex m_dataMutex;
};