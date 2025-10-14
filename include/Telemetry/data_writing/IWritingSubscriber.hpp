#pragma once
#include <memory>
#include "../data_acquisition/PacketParser.hpp"

class IWritingSubscriber : public std::enable_shared_from_this<IWritingSubscriber> {
public:
    virtual ~IWritingSubscriber() = default;
    virtual void onDataReceived(const PacketParser& packet) = 0;
};
