#include <filesystem> 
#include <iostream>
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/data_acquisition/SerialDataSource.hpp" 
#include "Telemetry/data_writing/DataManager.hpp"
#include "Telemetry/Services/DataAggregatorService.hpp"

AcquisitionMethod ServiceManager::m_method;
std::unique_ptr<SerialService> ServiceManager::m_serialService;
std::unique_ptr<NetworkService> ServiceManager::m_networkService;
std::unique_ptr<DataManager> ServiceManager::m_dataManager;
std::unique_ptr<DataAggregatorService> ServiceManager::m_aggregatorService;
std::shared_ptr<TxtWriter> ServiceManager::m_txtWriter = nullptr;
std::shared_ptr<CsvWriter> ServiceManager::m_csvWriter = nullptr;

void ServiceManager::initialize() {
    m_dataManager = std::make_unique<DataManager>();
    m_serialService = std::make_unique<SerialService>(m_dataManager.get());
    m_networkService = std::make_unique<NetworkService>();

    m_aggregatorService = std::make_unique<DataAggregatorService>();
    m_dataManager->addSubscriber(m_aggregatorService.get());

    std::cout << "ServiceManager initialized." << std::endl;
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
            if (m_serialService) success = m_serialService->start();
            break;
        case ACQUISITION_METHOD_NETWORK:
            if (m_networkService) success = m_networkService->start();
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
    
    if(m_aggregatorService) {
        m_aggregatorService->flush();
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
    m_aggregatorService.reset();
    m_dataManager.reset(); 
    std::cout << "ServiceManager cleaned up." << std::endl;
}

bool ServiceManager::startLogging(const std::string& outputDirectory) {
    std::filesystem::create_directories(outputDirectory);

    if (!m_txtWriter) {
        m_txtWriter = std::make_shared<TxtWriter>();
        if (m_txtWriter->createFile(outputDirectory)) {
            getDataManager()->addSubscriber(m_txtWriter.get());
            m_txtWriter->start();
        } else {
            m_txtWriter.reset();
        }
    }
    
    if (!m_csvWriter) {
        m_csvWriter = std::make_shared<CsvWriter>();
        if(m_csvWriter->createFile(outputDirectory, getAggregator()->getColumnOrder())) {
            getAggregator()->subscribe(m_csvWriter.get());
        } else {
            m_csvWriter.reset();
        }
    }

    if(m_txtWriter || m_csvWriter) {
         std::cout << "INFO: Logging started." << std::endl;
         return true;
    }
    
    std::cerr << "ERROR: Failed to start logging." << std::endl;
    return false;
}

void ServiceManager::stopLogging() {
    if (m_csvWriter) {
        getAggregator()->unsubscribe(m_csvWriter.get());
        m_csvWriter->stop();
        m_csvWriter.reset();
    }

    if (m_txtWriter) {
        getDataManager()->removeSubscriber(m_txtWriter.get());
        m_txtWriter->stop();
        m_txtWriter.reset();
    }
    
    std::cout << "INFO: Logging stopped." << std::endl;
}

bool ServiceManager::isLogging() {
    return m_txtWriter != nullptr || m_csvWriter != nullptr;
}

std::string ServiceManager::getCurrentLogFileName() {
    if (m_txtWriter) {
        return m_txtWriter->getCurrentFileName();
    }
    return "";
}