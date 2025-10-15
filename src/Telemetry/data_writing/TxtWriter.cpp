#include <iostream>
#include <variant>   
#include <chrono>
#include <iomanip>   
#include <sstream>
#include <filesystem> 
#include "Telemetry/data_writing/TxtWriter.hpp"

// Funzione helper per creare un nome di file univoco con un timestamp
static std::string generate_timestamped_filename() {
    const auto now = std::chrono::system_clock::now();
    const auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return "telemetry_" + ss.str() + ".txt";
}

TxtWriter::TxtWriter() = default;

TxtWriter::~TxtWriter() {
    stop();
}

void TxtWriter::closeFile() {
    if (m_outputFile.is_open()) {
        m_outputFile.close();
    }
}

bool TxtWriter::createFile(const std::string& directoryPath) {
    closeFile();

    m_currentFileName = generate_timestamped_filename();
    std::filesystem::path fullPath = std::filesystem::path(directoryPath) / m_currentFileName;

    m_outputFile.open(fullPath, std::ios::out | std::ios::trunc);
    if (!m_outputFile.is_open()) {
        std::cerr << "ERROR [TxtWriter]: Impossible to create file: " << fullPath << std::endl;
        m_currentFileName.clear();
        return false;
    }

    std::cout << "INFO [TxtWriter]: Recording started on file: " << fullPath << std::endl;
    m_outputFile << "timestamp;label;value" << std::endl;
    return true;
}

void TxtWriter::start() {
    if (!m_outputFile.is_open()) {
        std::cerr << "ERROR [TxtWriter]: Impossible to start, no file created. Call createFile() first." << std::endl;
        return;
    }
    m_shouldStop = false;
    
    // Svuota la coda da eventuali dati arrivati mentre la registrazione era ferma per registrare soloi nuovi dati
    m_queue.clear();
    
    // Avvia il thread che eseguirà processingLoop
    m_thread = std::thread(&TxtWriter::processingLoop, this);
}

void TxtWriter::stop() {
    m_shouldStop = true;
    
    // Attendere che il thread finisca la sua esecuzione
    if (m_thread.joinable()) {
        m_thread.join();
    }
    closeFile();
}

// Chiamato dal DataManager in modo veloce e non bloccante
void TxtWriter::onDataReceived(const PacketParser& packet) {
    // Non aggiunge pacchetti alla coda se si sta fermando
    if (!m_shouldStop) {
        m_queue.push(packet);
    }
}

void TxtWriter::processingLoop() {
    while (!m_shouldStop) {
        // Usa un'attesa con timeout, si sveglia se arriva dato o se scade il timeout
        if (auto optional_packet = m_queue.wait_for_and_pop(std::chrono::milliseconds(100))) {
            writePacketToFile(*optional_packet);
        }
    }

    // I pacchetti rimasti vengono svuotati velocemente
    std::cout << "INFO [TxtWriter]: Starting draining the queue..." << std::endl;
    while (auto optional_packet = m_queue.try_pop()) {
        writePacketToFile(*optional_packet);
    }
    std::cout << "INFO [TxtWriter]: Draining completed. Thread terminated." << std::endl;
}

void TxtWriter::writePacketToFile(const PacketParser& packet) {
    if (!m_outputFile.is_open()) return;

    auto tp = packet.timestamp;
    auto in_time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    m_outputFile << ss.str() << ";" << packet.label << ";";

    std::visit([this](auto&& arg) {
        m_outputFile << arg;
    }, packet.data);
    
    m_outputFile << std::endl;
    // Forza la scrittura su disco per non perdere dati in caso di crash.
    m_outputFile.flush();   
}

std::string TxtWriter::getCurrentFileName() const {
    return m_currentFileName;
}