/**
 * @file SerialDataSource.hpp
 * @brief Cross-platform serial port (UART / USB-serial) data source.
 *
 * @details
 * Implements IDataSource with the native API of each operating system:
 * Win32 (CreateFile/ReadFile) on Windows, termios on Linux and macOS (on macOS
 * the baud rate is set with the IOSSIOSPEED ioctl to support non-standard
 * speeds such as 500000). Fixed 8N1 configuration, no flow control,
 * non-blocking reads.
 */
#pragma once
#include <string>
#include <vector>
#include "IDataSource.hpp"
#include "BaudRateQueryable.hpp"

/**
 * @brief Serial port used as a source of telemetry bytes.
 */
class SerialDataSource: public IDataSource, public BaudRateQueryable {
public:
    SerialDataSource();
    /** @brief Closes the port if still open. */
    ~SerialDataSource() override;

    /**
     * @brief Lists the serial ports present in the system.
     *
     * - Windows: existing COM0..COM254 (QueryDosDevice).
     * - Linux: /dev/ttyUSB*, /dev/ttyACM*, /dev/ttyS*, /dev/rfcomm*.
     * - macOS: /dev/tty.* and /dev/cu.*.
     *
     * @return Alphabetically sorted list.
     */
    std::vector<std::string> getAvailableResources() override;
    /**
     * @brief Opens and configures the port (8N1, non-blocking) and flushes its buffers.
     * @param resource Port name/path.
     * @param baudRate Speed in bit/s (must be one of supportedBaudRates()).
     * @return true if the port is ready.
     */
    bool open(const std::string &resource, int baudRate) override;
    /**
     * @brief Reads up to 1024 available bytes without blocking.
     * @return Bytes read (empty if no data or on error).
     */
    std::vector<uint8_t> readPacket() override;
    /** @brief Closes the port. */
    void close() override;
    /** @brief Baud rates selectable in the UI (proposed default: 500000). */
    std::vector<int> supportedBaudRates() const override;

private:
#ifdef _WIN32
    void* handle = nullptr; ///< Win32 HANDLE of the port (nullptr when closed).
#else
    int fd = -1; ///< POSIX file descriptor of the port (-1 when closed).
#endif
    std::vector<std::string> m_availablePorts; ///< Last list of ports found.
};