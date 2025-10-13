#pragma once
#include <string>

struct NetworkConfig{
    std::string ipAddress;
    int port;
};

class NetworkService {
public:
    NetworkService() = default;
    bool configure(const NetworkConfig& config);
    bool start();
    void stop();
    bool isRunning() const;
};