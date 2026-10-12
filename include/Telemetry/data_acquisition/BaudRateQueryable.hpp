/**
 * @file BaudRateQueryable.hpp
 * @brief Interface for the sources that support choosing a baud rate.
 */
#pragma once
#include <vector>

/**
 * @brief Implemented by the data sources that can list their supported baud rates.
 *
 * Used by the SerialDeviceSelection window to fill the drop-down menu.
 */
class BaudRateQueryable {
public:
    virtual ~BaudRateQueryable() = default;
    
    /**
     * @brief Baud rates supported on the current platform.
     * @return List in bit/s, in ascending order.
     */
    virtual std::vector<int> supportedBaudRates() const = 0;
};