#pragma once
#include <string>
#include <vector>

class IDataSource {
public:
    IDataSource() = default;
    virtual ~IDataSource() = default;

    virtual std::vector<std::string> getAvailableResources() = 0;
    virtual bool open(const std::string &resource, int baudRate) = 0;
    virtual std::vector<uint8_t> readPacket() = 0;
    virtual void close() = 0;
};