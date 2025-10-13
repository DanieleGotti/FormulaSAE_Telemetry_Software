#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/data_acquisition/SerialDataSource.hpp" 

std::string ServiceManager::dbPath;
AcquisitionMethod ServiceManager::m_method;
std::unique_ptr<SerialService> ServiceManager::m_serialService;
std::unique_ptr<NetworkService> ServiceManager::m_networkService;


void ServiceManager::initialize() {
    m_serialService = std::make_unique<SerialService>();
    m_networkService = std::make_unique<NetworkService>();
    std::cout << "ServiceManager initialized." << std::endl;
}

std::vector<std::string> ServiceManager::getAllConnectionOptions() {
    // Per ora restituisce solo le opzioni seriali
    SerialDataSource sds;
    return sds.getAvailableResources();
}

void ServiceManager::setDBPath(const std::string& path) {
    dbPath = path;
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

void ServiceManager::startServices() {
    switch (m_method) {
        case ACQUISITION_METHOD_SERIAL:
            if (m_serialService) m_serialService->start();
            break;
        case ACQUISITION_METHOD_NETWORK:
            if (m_networkService) m_networkService->start();
            break;
        default:
            std::cerr << "ServiceManager: No acquisition method selected." << std::endl;
            break;
    }
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
    m_serialService.reset();
    m_networkService.reset();
    std::cout << "ServiceManager cleaned up." << std::endl;
}