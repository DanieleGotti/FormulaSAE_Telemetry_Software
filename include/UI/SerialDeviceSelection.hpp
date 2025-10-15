#pragma once
#include <string>
#include <vector>
#include <functional> 
#include "UIElement.hpp"

class SerialDeviceSelection : public UIElement {
public:
    // Sarà una funzione che accetta porta (stringa) e baudrate (intero)
    using ConnectCallback = std::function<void(const std::string&, int)>;
    // Quando di preme il pulsante "Connetti"
    explicit SerialDeviceSelection(ConnectCallback onConnectCallback);
    void draw() override;

private:
    void refreshPorts();

    ConnectCallback m_onConnectCallback;

    int m_selectedPortIndex = -1;
    int m_selectedBaudrateIndex = -1;
    std::vector<std::string> m_ports;
    std::vector<int> m_baudRates;
};