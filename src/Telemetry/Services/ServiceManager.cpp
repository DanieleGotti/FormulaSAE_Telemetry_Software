#include <filesystem> 
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/data_acquisition/SerialDataSource.hpp" 
#include "Telemetry/data_writing/DataManager.hpp"

AcquisitionMethod ServiceManager::m_method;
std::unique_ptr<SerialService> ServiceManager::m_serialService;
std::unique_ptr<NetworkService> ServiceManager::m_networkService;
std::unique_ptr<DataManager> ServiceManager::m_dataManager;
std::shared_ptr<TxtWriter> ServiceManager::m_txtWriter = nullptr;

void ServiceManager::initialize() {
    m_dataManager = std::make_unique<DataManager>();
    m_serialService = std::make_unique<SerialService>(m_dataManager.get());
    m_networkService = std::make_unique<NetworkService>();
    std::cout << "ServiceManager initialized." << std::endl;
}

std::vector<std::string> ServiceManager::getAllConnectionOptions() {
    SerialDataSource sds;
    return sds.getAvailableResources();
}

DataManager* ServiceManager::getDataManager() {
    return m_dataManager.get();
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

bool ServiceManager::startServices() {
    bool success = false;
    switch (m_method) {
        case ACQUISITION_METHOD_SERIAL:
            if (m_serialService) {
                success = m_serialService->start();
            }
            break;
        case ACQUISITION_METHOD_NETWORK:
            if (m_networkService) {
                success = m_networkService->start();
            }
            break;
        default:
            std::cerr << "ServiceManager: No acquisition method selected." << std::endl;
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
    std::cout << "All services stopped." << std::endl;
}

void ServiceManager::cleanup() {
    if (isLogging()) {
        stopLogging();
    }
    stopTasks();
    m_serialService.reset();
    m_networkService.reset();
    m_dataManager.reset(); 
    std::cout << "ServiceManager cleaned up." << std::endl;
}

bool ServiceManager::startLogging(const std::string& outputDirectory) {
    if (m_txtWriter) return true; 

    std::filesystem::create_directories(outputDirectory);
    m_txtWriter = std::make_shared<TxtWriter>();

    if (m_txtWriter->createFile(outputDirectory)) {
        if (getDataManager()) {
            getDataManager()->addSubscriber(m_txtWriter);
        }
        m_txtWriter->start();
        std::cout << "INFO: Logging started." << std::endl;
        return true;
    }
    
    m_txtWriter.reset();
    std::cerr << "ERROR: Failed to start logging." << std::endl;
    return false;
}

void ServiceManager::stopLogging() {
    if (!m_txtWriter) return;

    if (getDataManager()) {
        getDataManager()->removeSubscriber(m_txtWriter);
    }
    m_txtWriter->stop();
    m_txtWriter.reset();
    std::cout << "INFO: Logging stopped." << std::endl;
}

bool ServiceManager::isLogging() {
    return m_txtWriter != nullptr;
}

std::string ServiceManager::getCurrentLogFileName() {
    if (m_txtWriter) {
        return m_txtWriter->getCurrentFileName();
    }
    return "";
}