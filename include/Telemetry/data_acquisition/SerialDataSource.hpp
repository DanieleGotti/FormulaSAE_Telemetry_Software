#pragma once
#include <string>
#include <vector>
#include "IDataSource.hpp"
#include "BaudRateQueryable.hpp"

class SerialDataSource: public IDataSource, public BaudRateQueryable {
public:
    SerialDataSource();
    ~SerialDataSource() override;

    std::vector<std::string> getAvailableResources() override;
    bool open(const std::string &resource, int baudRate) override;
    std::vector<uint8_t> readPacket() override;
    void close() override;
    std::vector<int> supportedBaudRates() const override;

private:
#ifdef _WIN32
    void* handle = nullptr; // HANDLE for Windows
#else
    int fd = -1; // File descriptor for POSIX systems
#endif
    std::vector<std::string> m_availablePorts;
};