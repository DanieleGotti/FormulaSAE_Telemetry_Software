#pragma once
#include <string>
#include <vector>
#include <functional> 
#include "UIElement.hpp"

class UiManager;

class SerialDeviceSelection : public UIElement {
public:
    // Function that accepts a port and baudrate
    using ConnectCallback = std::function<void(const std::string&, int)>;
    // When the "Connect" button is pressed
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