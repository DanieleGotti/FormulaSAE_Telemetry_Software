/**
 * @file SerialService.hpp
 * @brief Live acquisition service from the serial port (two threads: reading + processing).
 *
 * @details
 * Data pipeline in live mode:
 * @code
 *  [acquisition thread]                  [processing thread]
 *  IDataSource::readPacket()  --push-->  ThreadSafeQueue  --pop-->  rxBuffer
 *                                                                      |
 *                                         PacketParser::parseLiveBytes()  (1 DbRow per A packet)
 *                                                                      |
 *                                         DataAggregator::processRow()   (adds statistics)
 *                                                                      |
 *                                         DataManager::processData()     (CSV + UI windows)
 * @endcode
 *
 * @warning The `onAggregatedDataReceived()` callbacks of the UI windows and of
 *          the CsvWriter run on the PROCESSING thread, not on the UI thread:
 *          implementations must protect their data with a mutex.
 */
#pragma once
#include <string>
#include <thread>
#include <memory>
#include "../data_acquisition/ThreadSafeQueue.hpp" 
#include "../data_acquisition/IDataSource.hpp"
#include "../data_acquisition/PacketParser.hpp"
#include "../data_writing/DataAggregator.hpp"

/**
 * @brief Serial connection parameters chosen by the user.
 */
struct SerialConfig {
    std::string port;   ///< Port name/path (e.g. "COM3", "/dev/ttyUSB0").
    int baudrate;       ///< Speed in bit/s (typically 500000).
};

/**
 * @brief Reads the serial port, decodes the packets and passes the rows to the DataAggregator.
 *
 * Life cycle: configure() -> start() -> ... -> stop(). Created and managed by ServiceManager.
 */
class SerialService {
public:
    /**
     * @brief Creates the service with a SerialDataSource as source.
     * @param aggregator Destination of the decoded rows (not owned, may be nullptr).
     */
    explicit SerialService(DataAggregator* aggregator);
    /** @brief Stops the threads and closes the port. */
    ~SerialService();

    /**
     * @brief Stores the port and baud rate to use on the next start().
     * @param config Connection parameters.
     * @return false if the service is already running (configuration ignored).
     */
    bool configure(const SerialConfig& config);
    /**
     * @brief Opens the port, resets parser and queue and starts the two threads.
     * @return true if started (or already running), false if the port cannot be opened.
     */
    bool start();
    /** @brief Stops the threads (join) and closes the port. Blocking. */
    void stop();
    /** @brief true between start() and stop(). */
    bool isRunning() const;

private:
    /** @brief Acquisition thread body: reads bytes and queues them (sleeps 1 ms when nothing arrives). */
    void acquisitionLoop();
    /** @brief Processing thread body: extracts packets from the buffer and forwards them to the aggregator. */
    void processingLoop();
    
    SerialConfig m_config;                       ///< Parameters of the last configure().
    bool m_running = false;                      ///< Running flag read by both threads.
    std::unique_ptr<IDataSource> m_dataSource;   ///< Byte source (SerialDataSource).
    std::thread m_thread; ///< Thread reading the data from the hardware.
    std::thread m_processingThread; ///< Thread decoding and forwarding to UI/log.
    
    /// Thread-safe queue carrying raw bytes from the acquisition thread to the processing thread.
    ThreadSafeQueue<std::vector<uint8_t>> m_queue; 
    
    DataAggregator* m_aggregator;  ///< Destination of the rows (owned by ServiceManager).
    PacketParser m_parser;         ///< Decoder used only by the processing thread.
};