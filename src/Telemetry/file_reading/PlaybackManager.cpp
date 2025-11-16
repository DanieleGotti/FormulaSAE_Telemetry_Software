#include <algorithm>
#include "Telemetry/file_reading/PlaybackManager.hpp"

void PlaybackManager::setData(std::vector<DbRow>&& data) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_aggregatedData = std::move(data);
    m_currentIndex = 0;
}

void PlaybackManager::clearData() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_aggregatedData.clear();
    m_currentIndex = 0;
}

void PlaybackManager::setCurrentIndex(size_t index) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (index < m_aggregatedData.size()) {
        m_currentIndex = index;
    }
}

size_t PlaybackManager::getCurrentIndex() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_currentIndex;
}

size_t PlaybackManager::getTotalRows() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_aggregatedData.size();
}

std::optional<DbRow> PlaybackManager::getCurrentRow() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (!m_aggregatedData.empty() && m_currentIndex < m_aggregatedData.size()) {
        return m_aggregatedData[m_currentIndex];
    }
    return std::nullopt;
}

std::optional<DbRow> PlaybackManager::getRowAtIndex(size_t index) const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (!m_aggregatedData.empty() && index < m_aggregatedData.size()) {
        return m_aggregatedData[index];
    }
    return std::nullopt;
}

std::vector<DbRow> PlaybackManager::getDataWindow(size_t centerIndex, size_t halfWindowSize) const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (m_aggregatedData.empty()) {
        return {};
    }

    size_t start = (centerIndex > halfWindowSize) ? (centerIndex - halfWindowSize) : 0;
    size_t end = std::min(m_aggregatedData.size(), centerIndex + halfWindowSize);

    return {m_aggregatedData.begin() + start, m_aggregatedData.begin() + end};
}