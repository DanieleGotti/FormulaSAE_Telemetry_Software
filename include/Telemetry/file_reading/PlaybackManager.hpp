#pragma once
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include "Telemetry/data_writing/DataAggregator.hpp" 

class PlaybackManager {
public:
    PlaybackManager() = default;

    // Method to load data
    void setData(std::vector<DbRow>&& data);
    void clearData();

    // Methods for navigation
    void setCurrentIndex(size_t index);
    size_t getCurrentIndex() const;
    size_t getTotalRows() const;

    // Gets the data row at the current position
    std::optional<DbRow> getCurrentRow() const;
    std::optional<DbRow> getRowAtIndex(size_t index) const;

    // Gets a window of data for the charts
    std::vector<DbRow> getDataWindow(size_t centerIndex, size_t halfWindowSize) const;

private:
    std::vector<DbRow> m_aggregatedData;
    size_t m_currentIndex = 0;
    mutable std::mutex m_dataMutex;
};