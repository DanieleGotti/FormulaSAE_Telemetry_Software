#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <imgui.h>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"

class UiManager;

class ModulesBatteryWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit ModulesBatteryWindow(UiManager* manager);
    ~ModulesBatteryWindow() override = default;

    void draw() override;
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    UiManager* m_uiManager;
    std::mutex m_dataMutex;
    std::map<std::string, std::string> m_latestData;

    // Helper per disegnare un singolo modulo
    void drawModuleCard(int index, const DbRow& dataMap);
    
    // Helper per ottenere il colore in base ai limiti (modificabile in futuro)
    ImVec4 getTempColor(float value);
    ImVec4 getVoltageColor(float value);
    
    // Helper per disegnare il LED di errore
    void drawErrorLed(bool isError);
};