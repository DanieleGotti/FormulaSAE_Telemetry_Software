#pragma once
#include <vector>
#include <map>
#include <string>
#include <mutex>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_acquisition/TelemetrySchema.hpp"

class UiManager;

class VarianceWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit VarianceWindow(UiManager* uiManager);
    ~VarianceWindow() override = default;

    void draw() override;
    void onAggregatedDataReceived(const DbRow& row) override;

private:
    UiManager* m_uiManager;
    std::mutex m_dataMutex;
    DbRow m_latestData;

    // Sourced from the schema, so it can never drift from the DataAggregator
    const std::vector<std::string>& m_targetSensors = TelemetrySchema::varianceChannels();
};