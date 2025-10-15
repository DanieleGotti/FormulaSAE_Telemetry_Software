#pragma once
#include <memory>
#include <string>
#include "../data_acquisition/PacketParser.hpp"

class IWritingSubscriber : public std::enable_shared_from_this<IWritingSubscriber> {
public:
    virtual ~IWritingSubscriber() = default;
    virtual bool createFile(const std::string& directoryPath) = 0;
    virtual void onDataReceived(const PacketParser& packet) = 0;
};
