#pragma once
#include <string>
#include <thread>
#include <memory>
#include "../data_acquisition/ThreadSafeQueue.hpp" 
#include "../data_acquisition/IDataSource.hpp"
#include "../data_acquisition/PacketParser.hpp"
#include "../data_writing/DataAggregator.hpp"

struct SerialConfig {
    std::string port;
    int baudrate;
};

class SerialService {
public:
    explicit SerialService(DataAggregator* aggregator);
    ~SerialService();

    bool configure(const SerialConfig& config);
    bool start();
    void stop();
    bool isRunning() const;

private:
    void acquisitionLoop();
    void processingLoop(); // NUOVO METODO
    
    SerialConfig m_config;
    bool m_running = false;
    std::unique_ptr<IDataSource> m_dataSource;
    std::thread m_thread; // Thread di lettura hardware
    std::thread m_processingThread; // NUOVO: Thread di elaborazione UI/Log
    
    // NUOVO: Coda sicura per passare i byte grezzi dal thread di lettura a quello di parsing
    ThreadSafeQueue<std::vector<uint8_t>> m_queue; 
    
    DataAggregator* m_aggregator;
    PacketParser m_parser;
};