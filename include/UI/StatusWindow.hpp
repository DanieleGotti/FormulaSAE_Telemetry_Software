#pragma once
#include <string>
#include <mutex>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_writing/DataAggregator.hpp"

class UiManager; 

class StatusWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit StatusWindow(UiManager* manager);
    ~StatusWindow() override = default;

    void draw() override;
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    void drawLed(const char* label, bool state);
    void drawBattery(int soc);
    
    float m_leftPaneWidth = 280.0f;

    UiManager* m_uiManager;
    std::mutex m_dataMutex;

    bool m_r2dButtonState = false;
    bool m_resetButtonState = false;
    bool m_sdcInputState = false;
    bool m_tsOnButtonState = false;
    std::string m_leftInverterFsm = "NULL";
    std::string m_rightInverterFsm = "NULL";
    std::string m_tractiveSystemFsm = "NULL"; 
    std::string m_ecuMode = "NULL";
    int m_stateOfCharge = 0;
};