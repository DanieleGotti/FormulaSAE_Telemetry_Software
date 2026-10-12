/**
 * @file IDataSource.hpp
 * @brief Abstract interface of a raw byte source (serial port, network in the future, ...).
 *
 * @details
 * SerialService works only through this interface, so adding a new physical
 * channel (e.g. raw USB, socket) only requires implementing it and passing it
 * to the service, without touching the parser or the UI. Current
 * implementation: SerialDataSource.
 */
#pragma once
#include <string>
#include <vector>

/**
 * @brief Raw data source the telemetry stream is read from.
 */
class IDataSource {
public:
    IDataSource() = default;
    virtual ~IDataSource() = default;

    /**
     * @brief Lists the resources that can be connected to (e.g. COM ports or /dev/tty*).
     * @return Names of the available resources.
     */
    virtual std::vector<std::string> getAvailableResources() = 0;
    /**
     * @brief Opens and configures the resource.
     * @param resource Resource name (as returned by getAvailableResources()).
     * @param baudRate Communication speed in bit/s.
     * @return true if opening succeeded.
     */
    virtual bool open(const std::string &resource, int baudRate) = 0;
    /**
     * @brief Reads the available bytes WITHOUT blocking.
     *
     * The result is NOT aligned to packets: it is an arbitrary chunk of the
     * stream, which the PacketParser puts back together.
     *
     * @return Bytes read (empty vector if nothing arrived or on error).
     */
    virtual std::vector<uint8_t> readPacket() = 0;
    /** @brief Closes the resource (safe to call even if already closed). */
    virtual void close() = 0;
};