#pragma once
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include "SerialService.hpp"
#include "NetworkService.hpp"

typedef enum {
    ACQUISITION_METHOD_SERIAL = 0,
    ACQUISITION_METHOD_NETWORK = 1
} AcquisitionMethod;

class ServiceManager {
public:
    static void initialize();
    static std::vector<std::string> getAllConnectionOptions();
    static void setDBPath(const std::string& path);
    static void setAcquisitionMethod(AcquisitionMethod method);
    static bool configureSerial(const std::string& port, int baudrate);
    static bool configureNetwork(const std::string& ip, int port);
    static void startServices();
    static void stopTasks();
    static void cleanup();

private:
    static std::string dbPath;
    static AcquisitionMethod m_method;
    static std::unique_ptr<SerialService> m_serialService;
    static std::unique_ptr<NetworkService> m_networkService;
};