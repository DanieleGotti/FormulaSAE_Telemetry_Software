#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_writing/DataAggregator.hpp"

class UiManager; 

class StatusWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit StatusWindow(UiManager* manager);
    ~StatusWindow() override = default;

    // Metodo da UIElement
    void draw() override;
    // Metodo da IAggregatedDataSubscriber
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    // Funzione helper per disegnare un LED con etichetta
    void drawLed(const char* label, bool state);
    // Disegna un singolo indicatore LED con un'etichetta
    void drawLedIndicator(const char* label, bool state);

    UiManager* m_uiManager;
    std::mutex m_dataMutex;

    bool m_r2dButtonState = false;
    bool m_resetButtonState = false;
    bool m_sdcInputState = false;
    bool m_tsOnButtonState = false;
    std::string m_leftInverterFsm = "NULL";
    std::string m_rightInverterFsm = "NULL";
};