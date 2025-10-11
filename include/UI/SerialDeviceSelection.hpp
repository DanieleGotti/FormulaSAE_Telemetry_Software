#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <bitset>
#include <iostream>

#include "Telemetry/data_acquisition/SerialDataSource.hpp"

#include "UIElement.hpp"

class SerialDeviceSelection : public UIElement {
public:
    SerialDeviceSelection() = default;
    void draw() override;

private:
    void refreshPorts();
    void appendLog(const std::string& msg);

private:
    SerialDataSource m_serial;
    bool m_connected = false;
    std::string m_selectedPort;
    int m_selectedBaudrate;
    int m_selectedIndex = 0;
    std::vector<std::string> m_ports;
    std::string m_log;

private:
    template < typename... Args >
    std::string sstr(Args&&... args) {
        std::ostringstream sstr;
        sstr << std::dec;
        (sstr << ... << args);
        return sstr.str();
    }
};