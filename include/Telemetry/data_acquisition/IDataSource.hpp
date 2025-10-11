#pragma once
#include <string>
#include <vector>

class IDataSource {
public:
    IDataSource() = default;
    virtual ~IDataSource() = default;

    // Lists all available data sources
    virtual std::vector<std::string> getAvailableResources() = 0;;

    // Connects to a specific data source
    virtual bool open(const std::string &resource, int baudRate) = 0;

    // Reads a data packet from the data source
    virtual std::vector<uint8_t> readPacket() = 0;

    // Disconnects from the data source
    virtual void close() = 0;
};