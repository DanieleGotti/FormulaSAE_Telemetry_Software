/**
 * @file ModulesBatteryWindow.cpp
 * @brief Implementation of the EMMA and battery modules window (see ModulesBatteryWindow.hpp).
 */
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include "UI/UIManager.hpp"
#include "UI/ModulesBatteryWindow.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

/**
 * @brief Rebuilds the hexadecimal EMMA error code from the text and prefixes it.
 *
 * E.g. "ERR_BSPD | ERR_DAISY_CHAIN" -> "(0x0180) ERR_BSPD | ERR_DAISY_CHAIN".
 * @note The bits must stay aligned with kEmmaFlags in TelemetrySchema.cpp.
 *
 * @param errStr Text produced by TelemetrySchema for the emma_error field.
 * @return Text with the hexadecimal code in front.
 */
static std::string formatEmmaError(const std::string& errStr) {
    if (errStr == "EMMA_STATUS_OK") return "(0x0000) EMMA_STATUS_OK";
    if (errStr == "Null" || errStr == "--" || errStr.empty()) return errStr;

    uint16_t hex = 0;
    if (errStr.find("ERR_UNDERVOLTAGE") != std::string::npos) hex |= 0x0001;
    if (errStr.find("ERR_OVERVOLTAGE") != std::string::npos) hex |= 0x0002;
    if (errStr.find("ERR_OVERTEMP_CELL") != std::string::npos) hex |= 0x0004;
    if (errStr.find("ERR_UNDERTEMP_CELL") != std::string::npos) hex |= 0x0008;
    if (errStr.find("ERR_OVERTEMP_PCB") != std::string::npos) hex |= 0x0010;
    if (errStr.find("ERR_UNDERTEMP_PCB") != std::string::npos) hex |= 0x0020;
    if (errStr.find("ERR_CELL_MISMATCH") != std::string::npos) hex |= 0x0040;
    if (errStr.find("ERR_BSPD") != std::string::npos) hex |= 0x0080;
    if (errStr.find("ERR_DAISY_CHAIN") != std::string::npos) hex |= 0x0100;
    if (errStr.find("IMD_AMS_LATCH") != std::string::npos) hex |= 0x0200;
    if (errStr.find("EMMA_FINISH_PRECHARGE") != std::string::npos) hex |= 0x0800;
    if (errStr.find("ERR_CRITICAL_FAULT") != std::string::npos) hex |= 0x8000;

    char buf[32];
    snprintf(buf, sizeof(buf), "(0x%04X) ", hex);
    return std::string(buf) + errStr;
}

ModulesBatteryWindow::ModulesBatteryWindow(UiManager* manager) : m_uiManager(manager) {}

void ModulesBatteryWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_latestData = dataRow;
}

ImVec4 ModulesBatteryWindow::getTempColor(float value) {
    if (value < 50.0f) return ImVec4(0.1f, 0.8f, 0.1f, 1.0f);       // green
    else if (value < 65.0f) return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);  // orange
    else return ImVec4(0.85f, 0.1f, 0.1f, 1.0f);                    // red
}

ImVec4 ModulesBatteryWindow::getVoltageColor(float value) {
    if (value >= 3.6f && value <= 4.2f) return ImVec4(0.1f, 0.8f, 0.1f, 1.0f);    // green
    else if (value > 3.0f && value < 4.5f) return ImVec4(1.0f, 0.5f, 0.0f, 1.0f); // orange
    else return ImVec4(0.85f, 0.1f, 0.1f, 1.0f);                                  // red
}

void ModulesBatteryWindow::printValue(const std::string& label, const std::string& key, const DbRow& dataMap, const std::string& unit) {
    ImGui::Text("%s:", label.c_str());

    std::string valStr = "Null";
    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        valStr = it->second;
    }

    std::string unitStr = unit.empty() ? "[-]" : "[" + unit + "]";

    // Measure value and unit to align them to the right edge of the window
    ImGui::PushFont(m_uiManager->font_body);
    float valWidth = ImGui::CalcTextSize(valStr.c_str()).x;
    float unitWidth = ImGui::CalcTextSize(unitStr.c_str()).x;
    ImGui::PopFont();

    float spacing = 5.0f;
    float rightEdge = ImGui::GetWindowContentRegionMax().x;

    ImGui::SameLine(rightEdge - unitWidth - spacing - valWidth);
    ImGui::PushFont(m_uiManager->font_body);
    ImGui::Text("%s", valStr.c_str());
    ImGui::PopFont();

    ImGui::SameLine(rightEdge - unitWidth);
    ImGui::PushFont(m_uiManager->font_body);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", unitStr.c_str());
    ImGui::PopFont();
}

void ModulesBatteryWindow::printErrorValue(const std::string& label, const DbRow& dataMap) {
    ImGui::Text("%s:", label.c_str()); 
    
    std::string formattedStr = "Null";
    ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);

    auto it = dataMap.find("emma_error");
    if (it != dataMap.end()) {
        formattedStr = formatEmmaError(it->second);
        // Green only for status OK or precharge finished; any other bit is an error
        if (it->second == "EMMA_STATUS_OK" || it->second == "EMMA_FINISH_PRECHARGE") {
            color = ImVec4(0.1f, 0.8f, 0.1f, 1.0f);
        } else {
            color = ImVec4(0.85f, 0.1f, 0.1f, 1.0f);
        }
    }

    ImGui::PushFont(m_uiManager->font_body);
    float textWidth = ImGui::CalcTextSize(formattedStr.c_str()).x;
    ImGui::PopFont();

    float rightEdge = ImGui::GetWindowContentRegionMax().x;

    ImGui::SameLine(rightEdge - textWidth);
    ImGui::PushFont(m_uiManager->font_body);
    ImGui::TextColored(color, "%s", formattedStr.c_str());
    ImGui::PopFont();
}

void ModulesBatteryWindow::drawModuleCard(int index, const DbRow& dataMap) {
    // Keys of the module fields (generated in fieldsB_v1() / expandDerivedKeys())
    std::string idxStr = std::to_string(index);
    std::string keyTmp1 = "Temperature1Module" + idxStr;
    std::string keyTmp2 = "Temperature2Module" + idxStr;
    std::string keyTens = "VoltageModule" + idxStr;
    std::string keyOnline = "OnlineModule" + idxStr;

    ImGui::PushID(index); // unique IDs for the widgets of every card

    ImGui::PushFont(m_uiManager->font_body);
    ImGui::Text("Module %d", index); 
    ImGui::PopFont();
    ImGui::Separator();

    // "Label  value [unit]" line with coloured value, right-aligned in the column
    auto drawValue = [&](const char* label, const std::string& key, bool isTemp) {
        ImGui::Text("%s", label);
        
        std::string valStr = "--";
        std::string unitStr = isTemp ? "[°C]" : "[V]";
        bool hasValidValue = false;
        ImVec4 valColor;

        auto it = dataMap.find(key);
        if (it != dataMap.end()) {
            try {
                float val = std::stof(it->second);
                valColor = isTemp ? getTempColor(val) : getVoltageColor(val);
                
                char valBuf[16];
                snprintf(valBuf, sizeof(valBuf), "%.1f", val);
                valStr = valBuf;
                hasValidValue = true;
            } catch (...) {
                valStr = "Null";
            }
        }

        ImGui::PushFont(m_uiManager->font_label);
        float valWidth = ImGui::CalcTextSize(valStr.c_str()).x;
        ImGui::PopFont();

        ImGui::PushFont(m_uiManager->font_body);
        float unitWidth = ImGui::CalcTextSize(unitStr.c_str()).x;
        ImGui::PopFont();

        float spacing = 5.0f;
        float totalRightWidth = valWidth + spacing + unitWidth;

        float columnWidth = ImGui::GetColumnWidth();
        float offsetX = columnWidth - totalRightWidth - 5.0f;

        // Keep the value from overlapping the label in narrow columns
        float minOffsetX = ImGui::CalcTextSize(label).x + 10.0f;
        if (offsetX < minOffsetX) {
            offsetX = minOffsetX;
        }

        ImGui::SameLine(offsetX);
        ImGui::PushFont(m_uiManager->font_body); 
        if (hasValidValue) {
            ImGui::TextColored(valColor, "%s", valStr.c_str()); 
        } else {
            ImGui::Text("%s", valStr.c_str());
        }
        ImGui::PopFont();

        ImGui::SameLine(offsetX + valWidth + spacing); 
        ImGui::PushFont(m_uiManager->font_body);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", unitStr.c_str());
        ImGui::PopFont();
    };
    
    // Online state LED: green "1", red "0", grey when the value is missing
    auto drawOnlineDot = [&](const char* label, const std::string& key) {
        ImGui::Text("%s", label);

        ImVec4 dotColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        auto it = dataMap.find(key);
        if (it != dataMap.end()) {
            dotColor = (it->second == "1") ? ImVec4(0.1f, 0.8f, 0.1f, 1.0f) : ImVec4(0.85f, 0.1f, 0.1f, 1.0f);
        }

        float radius = ImGui::GetFontSize() * 0.28f;
        float diameter = radius * 2.0f;

        float columnWidth = ImGui::GetColumnWidth();
        float offsetX = columnWidth - diameter - 5.0f;

        float minOffsetX = ImGui::CalcTextSize(label).x + 10.0f;
        if (offsetX < minOffsetX) {
            offsetX = minOffsetX;
        }

        ImGui::SameLine(offsetX);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 center(pos.x + radius, pos.y + ImGui::GetTextLineHeight() * 0.5f);
        ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(dotColor), 16);
        ImGui::Dummy(ImVec2(diameter, ImGui::GetTextLineHeight()));
    };

    ImGui::BeginGroup();
    drawValue("Temperature1:", keyTmp1, true);
    drawValue("Temperature2:", keyTmp2, true);
    drawValue("Voltage:", keyTens, false);
    drawOnlineDot("Online:", keyOnline);
    ImGui::EndGroup();
    
    ImGui::PopID();
}

void ModulesBatteryWindow::draw() {
    ImGui::Begin("Battery (EMMA)");

    DbRow dataToDisplay;
    bool hasData = false;

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (auto row = playbackManager->getCurrentRow()) {
            dataToDisplay = *row;
            hasData = true;
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        if (!m_latestData.empty()) {
            dataToDisplay = m_latestData;
            hasData = true;
        }
    }

    if (hasData) {
        // ---- EMMA status ----
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("EMMA status");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();
        
        printValue("Current", "emma_current", dataToDisplay, "A");
        printValue("Voltage", "emma_voltage", dataToDisplay, "V");
        printValue("Yaw", "emma_yaw", dataToDisplay, "°/s");
        printErrorValue("Error", dataToDisplay);
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // ---- Modules grid ----
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Modules data");
        ImGui::PopFont();
        ImGui::Separator();

        // Number of columns based on the width (at least 160 px per card, 1 to 14 columns)
        float availWidth = ImGui::GetContentRegionAvail().x;
        float minModuleWidth = 160.0f; 
        int columns = static_cast<int>(availWidth / minModuleWidth);
        
        if (columns < 1) columns = 1;
        if (columns > 14) columns = 14;
        if (ImGui::BeginTable("Modules grid", columns, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingStretchSame)) {
            for (int i = 1; i <= 14; ++i) {
                ImGui::TableNextColumn();
                drawModuleCard(i, dataToDisplay);
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::Text("Waiting for data.");
    }

    ImGui::End();
}