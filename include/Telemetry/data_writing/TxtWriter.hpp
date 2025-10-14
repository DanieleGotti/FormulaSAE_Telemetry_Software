#pragma once
#include <string>
#include <fstream>
#include <thread>
#include <atomic>
#include "IWritingSubscriber.hpp"
#include "../data_acquisition/ThreadSafeQueue.hpp" 
#include "../data_acquisition/PacketParser.hpp"

class TxtWriter : public IWritingSubscriber {
public:
    explicit TxtWriter(const std::string& filePath);
    ~TxtWriter() override;

    // No copia e spostamento per evitare problemi con thread e file
    TxtWriter(const TxtWriter&) = delete;
    TxtWriter& operator=(const TxtWriter&) = delete;

    void start();
    void stop();
    void onDataReceived(const PacketParser& packet) override;

private:
    void processingLoop();
    std::ofstream m_outputFile;
    ThreadSafeQueue<PacketParser> m_queue;
    std::thread m_thread;
    std::atomic<bool> m_isRunning{false};
};