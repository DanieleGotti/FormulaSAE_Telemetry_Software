#pragma once
#include <string>
#include <thread>
#include <memory>
#include "../data_acquisition/IDataSource.hpp"
#include "../data_writing/DataManager.hpp"

struct SerialConfig {
    std::string port;
    int baudrate;
};

class SerialService {
public:
    explicit SerialService(DataManager* dataManager);
    ~SerialService();

    bool configure(const SerialConfig& config);
    bool start();
    void stop();
    bool isRunning() const;

private:
    void acquisitionLoop();
    
    SerialConfig m_config;
    bool m_running = false;
    std::unique_ptr<IDataSource> m_dataSource;
    std::thread m_thread;
    DataManager* m_dataManager;
};