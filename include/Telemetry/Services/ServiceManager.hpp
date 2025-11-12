#pragma once
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include "SerialService.hpp"
#include "NetworkService.hpp"
#include "../data_writing/DataManager.hpp"
#include "../data_writing/TxtWriter.hpp" 
#include "../data_writing/CsvWriter.hpp"
#include "../Services/DataAggregatorService.hpp"
#include "../../application_services/SettingsManager.hpp"
#include "../../utils/IAssetManager.hpp"

typedef enum {
    ACQUISITION_METHOD_SERIAL = 0,
    ACQUISITION_METHOD_NETWORK = 1
} AcquisitionMethod;

class ServiceManager {
public:
    static void initialize();
    static std::vector<std::string> getAllConnectionOptions();
    static DataManager* getDataManager();
    static DataAggregatorService* getAggregator();
    static void setAcquisitionMethod(AcquisitionMethod method);
    static bool configureSerial(const std::string& port, int baudrate);
    static bool configureNetwork(const std::string& ip, int port);
    static bool startServices();
    static void stopTasks();
    static void cleanup();
    static bool startLogging(const std::string& outputDirectory);
    static void stopLogging();
    static bool isLogging();
    static std::shared_ptr<SettingsManager> getSettingsManager();
    static std::shared_ptr<IAssetManager> getAssetManager();
    static std::string getCurrentLogFileName();

private:
    static AcquisitionMethod m_method;
    static std::shared_ptr<IAssetManager> m_assetManager;
    static std::shared_ptr<SettingsManager> m_settingsManager;
    static std::unique_ptr<SerialService> m_serialService;
    static std::unique_ptr<NetworkService> m_networkService;
    static std::unique_ptr<DataAggregatorService> m_aggregatorService; 
    static std::unique_ptr<DataManager> m_dataManager;
    static std::shared_ptr<TxtWriter> m_txtWriter; 
    static std::shared_ptr<CsvWriter> m_csvWriter;
};