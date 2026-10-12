/**
 * @file DataManager.hpp
 * @brief Distributes the live rows to the subscribers: loggers at full speed, UI limited to 10 Hz.
 */
#pragma once
#include <vector>
#include <mutex>
#include "IAggregatedDataSubscriber.hpp" 

/**
 * @brief Publisher of the aggregated rows (observer pattern).
 *
 * There are two subscriber lists:
 *  - **UI subscribers**: receive at most one row every 100 ms (10 Hz), which is
 *    enough for the eye and does not overload rendering;
 *  - **log subscribers**: receive EVERY row (200 Hz), used by the CsvWriter.
 *
 * Pointers are not owned: whoever subscribes must unsubscribe before being
 * destroyed (see UiManager::resetToHome()).
 */
class DataManager {
public:
    /** @brief Subscribes a UI receiver (rows limited to 10 Hz). */
    void addSubscriber(IAggregatedDataSubscriber* subscriber);
    /** @brief Removes a UI receiver. */
    void removeSubscriber(IAggregatedDataSubscriber* subscriber);
    
    /** @brief Subscribes a logger (receives every row, 200 Hz). */
    void addLogSubscriber(IAggregatedDataSubscriber* subscriber);
    /** @brief Removes a logger. */
    void removeLogSubscriber(IAggregatedDataSubscriber* subscriber);
    
    /**
     * @brief Publishes a row: always to the loggers, to the UI subscribers only if 100 ms have passed.
     * @param row Complete row produced by the DataAggregator.
     */
    void processData(const DbRow& row);

private:
    std::vector<IAggregatedDataSubscriber*> m_subscribers;     ///< UI subscribers.
    std::vector<IAggregatedDataSubscriber*> m_logSubscribers;  ///< Logger subscribers.
    std::mutex m_subscriberMutex;                              ///< Protects both lists.
    
    /// Time of the last delivery to the UI, used to limit its rate.
    std::chrono::steady_clock::time_point m_lastUiUpdate;
};