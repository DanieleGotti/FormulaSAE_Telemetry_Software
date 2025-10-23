#pragma once
#include <string>
#include <vector>
#include <functional> 
#include "UIElement.hpp"

class UiManager;

class SerialDeviceSelection : public UIElement {
public:
    // Funzione che accetta porta (stringa) e baudrate (intero)
    using ConnectCallback = std::function<void(const std::string&, int)>;
    // Quando si preme il pulsante "Connetti"
    explicit SerialDeviceSelection(UiManager* manager, ConnectCallback onConnectCallback);
    void draw() override;

private:
    void refreshPorts();

    UiManager* m_uiManager; 
    ConnectCallback m_onConnectCallback;

    int m_selectedPortIndex = -1;
    int m_selectedBaudrateIndex = -1;
    std::vector<std::string> m_ports;
    std::vector<int> m_baudRates;
};