#pragma once
#include <mutex>
#include <map>
#include <string>
#include "UI/UIElement.hpp"
#include "Telemetry/data_writing/IAggregatedDataSubscriber.hpp" // Usa la nuova interfaccia
#include "Telemetry/data_writing/DataAggregator.hpp" // Per AggregatedDataRow

class Dashboard : public UIElement, public IAggregatedDataSubscriber {
public:
    Dashboard();
    void draw() override;
    
    // Metodo implementato dalla nuova interfaccia
    void onAggregatedDataReceived(const DbRow& dataRow) override;
    
private:
    const std::string OUTPUT_DIRECTORY = "../output_data"; 
    // La mappa ora conterrà direttamente le stringhe formattate
    std::map<std::string, std::string> m_latestData;
    std::mutex m_dataMutex;
};