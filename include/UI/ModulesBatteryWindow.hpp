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
    void printErrorValue(const std::string& label, const DbRow& dataMap);
    void printValue(const std::string& label, const std::string& key, const DbRow& dataMap, const std::string& unit = "");

    // Funzione per decodificare gli errori di EMMA
    std::string decodeEmmaError(uint16_t error);
};