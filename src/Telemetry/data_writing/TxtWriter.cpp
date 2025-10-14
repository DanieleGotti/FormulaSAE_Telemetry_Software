#include <iostream>
#include <variant>   
#include <chrono>
#include <iomanip>   
#include <sstream>
#include "Telemetry/data_writing/TxtWriter.hpp"

static std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
    auto in_time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
    
    // Millisecondi per maggiore precisione
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

TxtWriter::TxtWriter(const std::string& filePath) {
    m_outputFile.open(filePath, std::ios::out | std::ios::trunc); 
    if (!m_outputFile.is_open()) {
        std::cerr << "ERROR [TxtWriter]: Impossible to open the output file: " << filePath << std::endl;
    } else {
        // Scriviamo un'intestazione per chiarezza
        m_outputFile << "timestamp,label,value" << std::endl;
    }
}

TxtWriter::~TxtWriter() {
    if (m_isRunning) {
        stop(); // Assicuriamoci che il thread sia fermato correttamente
    }
    if (m_outputFile.is_open()) {
        m_outputFile.close();
    }
}

void TxtWriter::start() {
    if (m_isRunning) return; // Già avviato
    if (!m_outputFile.is_open()) {
        std::cerr << "ERROR [TxtWriter]: Impossible to start, file not open." << std::endl;
        return;
    }

    m_isRunning = true;
    m_thread = std::thread(&TxtWriter::processingLoop, this);
    std::cout << "INFO [TxtWriter]: Writing service started." << std::endl;
}

void TxtWriter::stop() {
    if (!m_isRunning) return; 

    m_isRunning = false;
    m_queue.stop(); 

    if (m_thread.joinable()) {
        m_thread.join(); 
    }
    std::cout << "INFO [TxtWriter]: Writing service stopped." << std::endl;
}

void TxtWriter::onDataReceived(const PacketParser& packet) {
    if (m_isRunning) {
        m_queue.push(packet); 
    }
}

void TxtWriter::processingLoop() {
    while (m_isRunning) {
        // Si blocca qui finché non c'è un pacchetto o la coda viene fermata
        auto optional_packet = m_queue.wait_and_pop();

        if (optional_packet) {
            // Se riceve un pacchetto
            const auto& packet = *optional_packet;
            
            // Formattiamo la riga come: timestamp,etichetta,valore
            std::string timestamp_str = format_timestamp(packet.timestamp);
            m_outputFile << timestamp_str << ";" << packet.label << ";";

            std::visit([this](auto&& arg) {
                m_outputFile << arg;
            }, packet.data);
            
            m_outputFile << std::endl; 
        }
    }
}