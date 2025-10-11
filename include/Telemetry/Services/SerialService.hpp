#pragma once
#include <string>

struct SerialConfig {
    std::string port;
    int baudrate;
};

class SerialService {
public:
    SerialService() = default;
    bool configure(const SerialConfig& config);
    bool start();
    void stop();
    bool isRunning() const;
private:
    SerialConfig m_config;
    bool m_running = false;
};