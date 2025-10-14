#pragma once
#include "../data_acquisition/PacketParser.hpp"

class IWritingSubscriber {
public:
    virtual ~IWritingSubscriber() = default;
    virtual void onDataReceived(const ParsedPacket& packet) = 0;
};
