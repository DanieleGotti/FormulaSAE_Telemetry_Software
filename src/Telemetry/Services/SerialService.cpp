#include <iostream>
#include <chrono>
#include "Telemetry/Services/SerialService.hpp"
#include "Telemetry/data_acquisition/SerialDataSource.hpp"

SerialService::SerialService(DataAggregator* aggregator)
    : m_dataSource(std::make_unique<SerialDataSource>()), m_aggregator(aggregator) {}

SerialService::~SerialService() { stop(); }

bool SerialService::configure(const SerialConfig& config) {
    if (m_running) return false;
    m_config = config;
    return true;
}

bool SerialService::start() {
    if (m_running) return true;

    if (!m_dataSource->open(m_config.port, m_config.baudrate)) {
        std::cerr << "ERRORE[SerialService]: Impossibile aprire la porta." << std::endl;
        return false;
    }

    m_parser.resetCounters();
    m_queue.clear(); // Pulisce la coda se c'era sporcizia precedente
    m_running = true;
    m_thread = std::thread(&SerialService::acquisitionLoop, this);
    m_processingThread = std::thread(&SerialService::processingLoop, this); // Avvia il secondo thread
    std::cout << "INFO [SerialService]: Avviato sulla porta " << m_config.port << std::endl;
    return true;
}

void SerialService::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
    if (m_processingThread.joinable()) m_processingThread.join(); // Attende la chiusura del secondo thread
    m_dataSource->close();
}

bool SerialService::isRunning() const { return m_running; }

void SerialService::acquisitionLoop() {
    while (m_running) {
        std::vector<uint8_t> rawData = m_dataSource->readPacket();
        if (!rawData.empty()) {
            // Mette i dati in coda IMMEDIATAMENTE, senza aspettare Parsing o UI
            m_queue.push(std::move(rawData));
        } else {
            // Dorme pochissimo per non bruciare la CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void SerialService::processingLoop() {
    std::vector<uint8_t> rxBuffer;
    rxBuffer.reserve(8192); // Buffer largo per evitare riallocazioni
    
    while (m_running) {
        // Aspetta fino a 10ms che arrivino dati
        auto dataOpt = m_queue.wait_for_and_pop(std::chrono::milliseconds(10));
        
        if (dataOpt.has_value()) {
            // Accoda i nuovi byte
            rxBuffer.insert(rxBuffer.end(), dataOpt->begin(), dataOpt->end());

            DbRow readyRow;
            // Estrae tutti i pacchetti completi (il parser ci metterà il tempo che serve)
            while (m_parser.parseLiveBytes(rxBuffer, readyRow)) {
                if (m_aggregator) {
                    m_aggregator->processRow(readyRow); // Questo ora può fermarsi sui Mutex della UI in totale sicurezza!
                }
            }
        }
    }
}