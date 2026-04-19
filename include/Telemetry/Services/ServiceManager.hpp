#pragma once
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include "SerialService.hpp"
#include "NetworkService.hpp"
#include "Telemetry/data_writing/DataManager.hpp"
#include "Telemetry/data_writing/CsvWriter.hpp"
#include "Telemetry/data_writing/DataAggregator.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"
#include "Telemetry/Services/FileService.hpp"
#include "../../application_services/SettingsManager.hpp"
#include "../../utils/IAssetManager.hpp"

typedef enum {
    ACQUISITION_METHOD_SERIAL = 0,
    ACQUISITION_METHOD_NETWORK = 1,
    ACQUISITION_METHOD_FILE = 2
} AcquisitionMethod;

class ServiceManager {
public:
    static void initialize();
    static std::vector<std::string> getAllConnectionOptions();
    
    // I getter necessari
    static DataManager* getDataManager();
    static PlaybackManager* getPlaybackManager();
    static FileService* getFileService();
    
    // Configurazione e avvio
    static void setAcquisitionMethod(AcquisitionMethod method);
    static bool configureSerial(const std::string& port, int baudrate);
    static bool configureNetwork(const std::string& ip, int port);
    static bool configureFile(const std::string& filePath);
    
    static bool startServices();
    static void stopTasks();
    static void cleanup();
    
    // Registrazione
    static bool startLogging(const std::string& outputDirectory);
    static void stopLogging();
    static bool isLogging();
    static void resetForNewSession();
    
    static std::shared_ptr<SettingsManager> getSettingsManager();
    static std::shared_ptr<IAssetManager> getAssetManager();

private:
    static AcquisitionMethod m_method;
    static std::shared_ptr<IAssetManager> m_assetManager;
    static std::shared_ptr<SettingsManager> m_settingsManager;
    
    // Servizi principali
    static std::unique_ptr<SerialService> m_serialService;
    static std::unique_ptr<NetworkService> m_networkService;
    static std::unique_ptr<PlaybackManager> m_playbackManager;
    static std::unique_ptr<FileService> m_fileService;
    
    // Gestione Dati
    static std::unique_ptr<DataAggregator> m_dataAggregator;
    static std::unique_ptr<DataManager> m_dataManager;
    static std::shared_ptr<CsvWriter> m_csvWriter;
};