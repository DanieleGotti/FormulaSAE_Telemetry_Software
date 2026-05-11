#pragma once
#include <mutex>
#include <map>
#include <string>
#include <imgui.h> 
#include "UI/UIElement.hpp"
#include "Telemetry/data_writing/IAggregatedDataSubscriber.hpp" 

class UiManager; 

class InverterDataWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit InverterDataWindow(UiManager* manager); 
    
    void draw() override;
    void onAggregatedDataReceived(const DbRow& dataRow) override;
    
private:
    void printValue(const std::string& label, const std::string& key, const DbRow& dataMap, const std::string& unit = "");
    void printColoredValue(const std::string& label, const std::string& key, const DbRow& dataMap, float limitGreen, float limitRed);
    ImVec4 getColorForThreshold(float value, float limitGreen, float limitRed);

    UiManager* m_uiManager;    
    std::map<std::string, std::string> m_latestData;
    std::mutex m_dataMutex;
};