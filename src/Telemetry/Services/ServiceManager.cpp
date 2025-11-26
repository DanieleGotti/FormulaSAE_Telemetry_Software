#include <filesystem> 
#include <iostream>
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/Services/FileService.hpp"
#include "Telemetry/data_acquisition/SerialDataSource.hpp" 
#include "Telemetry/data_writing/DataManager.hpp"
#include "Telemetry/Services/DataAggregatorService.hpp"
#include "utils/AssetManagerFactory.hpp"
#include "utils/IAssetManager.hpp"

AcquisitionMethod ServiceManager::m_method;
std::shared_ptr<IAssetManager> ServiceManager::m_assetManager = nullptr;
std::shared_ptr<SettingsManager> ServiceManager::m_settingsManager = nullptr;
std::unique_ptr<DataManager> ServiceManager::m_dataManager = nullptr;
std::unique_ptr<DataAggregatorService> ServiceManager::m_aggregatorService = nullptr;
std::unique_ptr<SerialService> ServiceManager::m_serialService = nullptr;
std::unique_ptr<NetworkService> ServiceManager::m_networkService = nullptr;
std::unique_ptr<FileService> ServiceManager::m_fileService = nullptr;           
std::unique_ptr<PlaybackManager> ServiceManager::m_playbackManager = nullptr; 
std::shared_ptr<TxtWriter> ServiceManager::m_txtWriter = nullptr;
std::shared_ptr<CsvWriter> ServiceManager::m_csvWriter = nullptr;

void ServiceManager::initialize() {
    m_assetManager = CreateAssetManager();
    m_settingsManager = std::make_shared<SettingsManager>();
    m_dataManager = std::make_unique<DataManager>();
    m_serialService = std::make_unique<SerialService>(m_dataManager.get());
    m_networkService = std::make_unique<NetworkService>();
    m_fileService = std::make_unique<FileService>();         
    m_playbackManager = std::make_unique<PlaybackManager>();                   

    m_aggregatorService = std::make_unique<DataAggregatorService>();
    m_dataManager->addSubscriber(m_aggregatorService.get());

    std::cout << "INFO [ServiceManager]: ServiceManager inizializzato." << std::endl;
}

std::vector<std::string> ServiceManager::getAllConnectionOptions() {
    SerialDataSource sds;
    return sds.getAvailableResources();
}

DataManager* ServiceManager::getDataManager() {
    return m_dataManager.get();
}

DataAggregatorService* ServiceManager::getAggregator() {
    return m_aggregatorService.get();
}

PlaybackManager* ServiceManager::getPlaybackManager() {
    return m_playbackManager.get();
}

FileService* ServiceManager::getFileService() {
    return m_fileService.get();
}

void ServiceManager::setAcquisitionMethod(AcquisitionMethod method) {
    m_method = method;
}

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
            if (m_fileService) {
                m_fileService->startLoading();
                success = true; 
            }
            break;
        default:
            std::cerr << "ERRORE [ServiceManager]: Nessun metodo di acquisizione selezionato." << std::endl;
            success = false;
            break;
    }
    return success;
}

void ServiceManager::stopTasks() {
    if (m_serialService && m_serialService->isRunning()) {
        m_serialService->stop();
    }
    if (m_networkService && m_networkService->isRunning()) {
        m_networkService->stop();
    }
    
    if(m_aggregatorService) {
        m_aggregatorService->flush();
    }

    std::cout << "INFO [ServiceManager]: Tutti i task attivi sono stati fermati." << std::endl;
}

void ServiceManager::resetForNewSession() {
    std::cout << "INFO [ServiceManager]: Reset della sessione in corso." << std::endl;
    
    stopTasks();
    stopLogging();

    if (m_playbackManager) {
        m_playbackManager->clearData();
    }

    if (m_dataManager) {
        m_serialService = std::make_unique<SerialService>(m_dataManager.get());
        m_fileService = std::make_unique<FileService>();
        m_networkService = std::make_unique<NetworkService>();
    }
 
    if (m_dataManager && m_aggregatorService) {
        m_dataManager->removeSubscriber(m_aggregatorService.get());
        m_aggregatorService = std::make_unique<DataAggregatorService>();
        m_dataManager->addSubscriber(m_aggregatorService.get());
    }

    std::cout << "INFO [ServiceManager]: Sessione resettata. Pronto per una nuova configurazione." << std::endl;
}

void ServiceManager::cleanup() {
    resetForNewSession();

    m_aggregatorService.reset();
    m_dataManager.reset(); 
    m_playbackManager.reset();
    m_settingsManager.reset();
    m_assetManager.reset();

    std::cout << "INFO [ServiceManager]: ServiceManager pulito." << std::endl;
}

bool ServiceManager::startLogging(const std::string& logIdentifier) {
    if (m_method == ACQUISITION_METHOD_FILE) {
        // Modalità lettura file
        std::filesystem::path inputPath(logIdentifier);
        std::filesystem::path outputDir = "../output_data";
        std::filesystem::create_directories(outputDir);
        std::string csvName = inputPath.stem().string() + ".csv";
        std::filesystem::path fullCsvPath = outputDir / csvName;

        if (!m_csvWriter) {
            m_csvWriter = std::make_shared<CsvWriter>();
            if(m_csvWriter->createFile(fullCsvPath.string(), getAggregator()->getColumnOrder())) {
                getAggregator()->subscribe(m_csvWriter.get());
                std::cout << "REGISTRAZIONE [ServiceManager]: Generazione .csv avviata." << std::endl;
                return true;
            }
            m_csvWriter.reset();
        }
    } else {
        // Modalità seriale/rete
        const std::string& outputDirectory = logIdentifier;
        std::filesystem::create_directories(outputDirectory);

        bool txtOk = false;
        if (!m_txtWriter) {
            m_txtWriter = std::make_shared<TxtWriter>();
            if (m_txtWriter->createFile(outputDirectory)) {
                getDataManager()->addSubscriber(m_txtWriter.get());
                m_txtWriter->start();
                txtOk = true;
            } else {
                m_txtWriter.reset();
            }
        }

        bool csvOk = false;
        if (!m_csvWriter) {
            m_csvWriter = std::make_shared<CsvWriter>();
            if(m_csvWriter->createFile(outputDirectory, getAggregator()->getColumnOrder())) {
                getAggregator()->subscribe(m_csvWriter.get());
                csvOk = true;
            } else {
                m_csvWriter.reset();
            }
        }

        if(txtOk || csvOk) {
            std::cout << "REGISTRAZIONE [ServiceManager]: Registrazione avviata." << std::endl;
            return true;
        }
    }
    
    std::cerr << "ERRORE [ServiceManager]: Avvio registrazione fallito." << std::endl;
    return false;
}

void ServiceManager::stopLogging() {
    if (m_txtWriter) {
        getDataManager()->removeSubscriber(m_txtWriter.get());
        m_txtWriter->stop();
        m_txtWriter.reset();
    }

    if (m_csvWriter) {
        if (m_aggregatorService) {
            m_aggregatorService->flush();
        }
        getAggregator()->unsubscribe(m_csvWriter.get());
        m_csvWriter->stop();
        m_csvWriter.reset();

        std::cout << "REGISTRAZIONE [ServiceManager]: Registrazione fermata." << std::endl;
    }
}

bool ServiceManager::isLogging() {
    return m_txtWriter != nullptr || m_csvWriter != nullptr;
}

std::shared_ptr<SettingsManager> ServiceManager::getSettingsManager() {
    return m_settingsManager;
}

std::string ServiceManager::getCurrentLogFileName() {
    if (m_txtWriter) {
        return m_txtWriter->getCurrentFileName();
    }
    return "";
}

std::shared_ptr<IAssetManager> ServiceManager::getAssetManager() {
    if (!m_assetManager) {
        m_assetManager = CreateAssetManager();
    }
    return m_assetManager;
}

