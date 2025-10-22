#pragma once
#include "DataAggregator.hpp" 

class IAggregatedDataSubscriber {
public:
    virtual ~IAggregatedDataSubscriber() = default;
    virtual void onAggregatedDataReceived(const DbRow& dataRow) = 0;
};