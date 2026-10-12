/**
 * @file NetworkService.hpp
 * @brief Placeholder for a future network acquisition service (NOT implemented).
 *
 * @details
 * All methods are empty (see NetworkService.cpp). The simplest way to
 * implement it is to mirror SerialService: create a network IDataSource
 * (UDP/TCP socket) and reuse PacketParser + DataAggregator. The UI does not
 * currently offer a network connection option.
 */
#pragma once
#include <string>

/**
 * @brief Parameters of a network connection.
 */
struct NetworkConfig{
    std::string ipAddress;  ///< IP address of the remote device.
    int port;               ///< TCP/UDP port.
};

/**
 * @brief Network acquisition service. Stub: it does nothing.
 */
class NetworkService {
public:
    NetworkService() = default;
    
    /** @brief Stores the configuration (currently ignored). @return always true. */
    bool configure(const NetworkConfig& config);
    /** @brief Starts the acquisition (currently does nothing). @return always true. */
    bool start();
    /** @brief Stops the acquisition (currently does nothing). */
    void stop();
    /** @brief Running state. @return always false. */
    bool isRunning() const;
};