/**
 * @file ServiceManager.cpp
 * @brief Implementation of the static service registry (see ServiceManager.hpp).
 */
#include <filesystem>
#include <iostream>
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/data_acquisition/SerialDataSource.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"
#include "utils/AssetManagerFactory.hpp"

// Static member definitions (all empty until initialize() is called)
AcquisitionMethod ServiceManager::m_method;
std::shared_ptr<IAssetManager> ServiceManager::m_assetManager = nullptr;
std::shared_ptr<SettingsManager> ServiceManager::m_settingsManager = nullptr;
std::unique_ptr<DataManager> ServiceManager::m_dataManager = nullptr;
std::unique_ptr<DataAggregator> ServiceManager::m_dataAggregator = nullptr;
std::unique_ptr<SerialService> ServiceManager::m_serialService = nullptr;
std::unique_ptr<NetworkService> ServiceManager::m_networkService = nullptr;
std::unique_ptr<FileService> ServiceManager::m_fileService = nullptr;           
std::unique_ptr<PlaybackManager> ServiceManager::m_playbackManager = nullptr; 
std::shared_ptr<CsvWriter> ServiceManager::m_csvWriter = nullptr;

void ServiceManager::initialize() {
    m_assetManager = CreateAssetManager();
    m_settingsManager = std::make_shared<SettingsManager>();
    
    m_dataManager = std::make_unique<DataManager>();
    
    // The DataAggregator receives a lambda: every time it finishes adding the
    // statistics to a row, it passes the complete row to the DataManager
    m_dataAggregator = std::make_unique<DataAggregator>([](const DbRow& row) {
        if (m_dataManager) m_dataManager->processData(row);
    });

    m_serialService = std::make_unique<SerialService>(m_dataAggregator.get());
    m_networkService = std::make_unique<NetworkService>();
    m_fileService = std::make_unique<FileService>();         
    m_playbackManager = std::make_unique<PlaybackManager>();                   

    std::cout << "INFO [ServiceManager]: ServiceManager initialized." << std::endl;
}

std::vector<std::string> ServiceManager::getAllConnectionOptions() {
    // Temporary instance used only to enumerate the ports
    SerialDataSource sds;
    return sds.getAvailableResources();
}

DataManager* ServiceManager::getDataManager() { return m_dataManager.get(); }
PlaybackManager* ServiceManager::getPlaybackManager() { return m_playbackManager.get(); }
FileService* ServiceManager::getFileService() { return m_fileService.get(); }

void ServiceManager::setAcquisitionMethod(AcquisitionMethod method) { m_method = method; }

bool ServiceManager::configureSerial(const std::string& port, int baudrate) {
    if (!m_serialService) return false;
    SerialConfig config{port, baudrate};
    return m_serialService->configure(config);
}

bool ServiceManager::configureNetwork(const std::string& ip, int port) {
    if (!m_networkService) return false;
    NetworkConfig config{ip, port};
    return m_networkService->configure(config);
}

bool ServiceManager::configureFile(const std::string& filePath) {
    if (!m_fileService) return false;
    FileConfig config{filePath};
    return m_fileService->configure(config);
}

bool ServiceManager::startServices() {
    bool success = false;
    switch (m_method) {
        case ACQUISITION_METHOD_SERIAL:
            if (m_serialService) success = m_serialService->start();
            break;
        case ACQUISITION_METHOD_NETWORK:
            if (m_networkService) success = m_networkService->start();
            break;
        case ACQUISITION_METHOD_FILE:
            // Loading runs in the background: success is checked later with isFinished()
            if (m_fileService) {
                m_fileService->startLoading();
                success = true; 
            }
            break;
        default:
            std::cerr << "ERROR [ServiceManager]: No acquisition method selected." << std::endl;
            break;
    }
    return success;
}

void ServiceManager::stopTasks() {
    if (m_serialService && m_serialService->isRunning()) m_serialService->stop();
    if (m_networkService && m_networkService->isRunning()) m_networkService->stop();
    std::cout << "INFO [ServiceManager]: All active tasks have been stopped." << std::endl;
}

void ServiceManager::resetForNewSession() {
    std::cout << "INFO [ServiceManager]: Reset of the session in progress." << std::endl;
    stopTasks();
    stopLogging();

    // Safely cancel any running CSV export, to avoid orphan threads
    if (m_playbackManager) {
        m_playbackManager->cancelExport();
        m_playbackManager->clearData();
    }
    
    if (m_dataAggregator) m_dataAggregator->reset();

    // Recreate the services, linking them to the aggregator again
    if (m_dataAggregator) {
        m_serialService = std::make_unique<SerialService>(m_dataAggregator.get());
    }
    
    // Assigning a new unique_ptr destroys the old FileService: its destructor
    // stops and joins any pending loading thread.
    m_fileService = std::make_unique<FileService>();
    m_networkService = std::make_unique<NetworkService>();

    std::cout << "INFO [ServiceManager]: Session reset." << std::endl;
}

void ServiceManager::cleanup() {
    resetForNewSession();
    m_dataAggregator.reset();
    m_dataManager.reset(); 
    m_playbackManager.reset();
    m_settingsManager.reset();
    m_assetManager.reset();
    std::cout << "INFO [ServiceManager]: ServiceManager cleaned up." << std::endl;
}

bool ServiceManager::startLogging(const std::string& logIdentifier) {
    if (m_method == ACQUISITION_METHOD_FILE) {
        // Nothing is recorded from file: CSV conversion is done by the PlaybackManager export
        return true; 
    } else {
        std::filesystem::create_directories(logIdentifier);
        if (!m_csvWriter) {
            m_csvWriter = std::make_shared<CsvWriter>();
            if(m_csvWriter->createFile(logIdentifier, PacketParser::getColumnOrder())) {
                // Registered as a "log subscriber": it receives EVERY row (200 Hz), not only 10 Hz
                getDataManager()->addLogSubscriber(m_csvWriter.get());
                std::cout << "LOGGING [ServiceManager]: CSV generation started." << std::endl;
                return true;
            }
            m_csvWriter.reset();
        }
    }
    std::cerr << "ERROR [ServiceManager]: Failed to start logging." << std::endl;
    return false;
}

void ServiceManager::stopLogging() {
    if (m_csvWriter) {
        getDataManager()->removeLogSubscriber(m_csvWriter.get());
        m_csvWriter->stop();
        m_csvWriter.reset();
        std::cout << "LOGGING [ServiceManager]: CSV generation stopped." << std::endl;
    }
}

bool ServiceManager::isLogging() {
    return m_csvWriter != nullptr;
}

std::shared_ptr<SettingsManager> ServiceManager::getSettingsManager() { return m_settingsManager; }
std::shared_ptr<IAssetManager> ServiceManager::getAssetManager() {
    if (!m_assetManager) m_assetManager = CreateAssetManager();
    return m_assetManager;
}