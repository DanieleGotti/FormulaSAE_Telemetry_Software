/**
 * @file IAggregatedDataSubscriber.hpp
 * @brief "Observer" interface to receive the live telemetry rows, statistics included.
 *
 * @details
 * Implemented by every UI window showing live data and by the CsvWriter.
 * Objects subscribe to the DataManager (addSubscriber() for the UI at 10 Hz,
 * addLogSubscriber() for the loggers at 200 Hz).
 */
#pragma once
#include "DataAggregator.hpp" 

/**
 * @brief Receiver of the rows published by the DataManager.
 */
class IAggregatedDataSubscriber {
public:
    virtual ~IAggregatedDataSubscriber() = default;
    /**
     * @brief Called for every published row.
     *
     * @warning It runs on the serial processing thread, NOT on the UI thread:
     *          copy the needed data under a mutex and leave drawing to draw().
     *          It must be fast (it blocks the acquisition pipeline).
     *
     * @param dataRow Complete row (packet fields + statistics + counters).
     */
    virtual void onAggregatedDataReceived(const DbRow& dataRow) = 0;
};