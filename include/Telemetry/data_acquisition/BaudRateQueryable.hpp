#pragma once
#include <vector>

class BaudRateQueryable {
public:
    virtual ~BaudRateQueryable() = default;
    
    virtual std::vector<int> supportedBaudRates() const = 0;
};