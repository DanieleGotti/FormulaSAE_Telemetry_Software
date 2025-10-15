#pragma once
#include <string>
#include <fstream>
#include <thread>
#include <atomic> 
#include "Telemetry/data_writing/IWritingSubscriber.hpp"
#include "Telemetry/data_acquisition/ThreadSafeQueue.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"

class TxtWriter : public IWritingSubscriber {
public:
    TxtWriter();
    ~TxtWriter() override;

    // No copia e spostamento per sicurezza, dato che gestisce un thread
    TxtWriter(const TxtWriter&) = delete;
    TxtWriter& operator=(const TxtWriter&) = delete;
    TxtWriter(TxtWriter&&) = delete;
    TxtWriter& operator=(TxtWriter&&) = delete;

    bool createFile(const std::string& directoryPath) override;
    // Metodo chiamato dal DataManager per ricevere i dati
    void onDataReceived(const PacketParser& packet) override;
    void start();
    void stop();
    std::string getCurrentFileName() const;

private:
    // Funzione eseguita dal thread per elaborare i pacchetti in coda
    void processingLoop();
    // Scrive un singolo pacchetto formattato sul file di output
    void writePacketToFile(const PacketParser& packet);
    void closeFile();

    std::ofstream m_outputFile;
    ThreadSafeQueue<PacketParser> m_queue;
    std::thread m_thread;
    std::atomic<bool> m_shouldStop{false};
    std::string m_currentFileName;
};