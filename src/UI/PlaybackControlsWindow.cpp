/**
 * @file PlaybackControlsWindow.cpp
 * @brief Implementation of the playback controls (see PlaybackControlsWindow.hpp).
 */
#include <imgui.h>
#include <string>
#include <optional>
#include <algorithm> 
#include "UI/UIManager.hpp"
#include "UI/PlaybackControlsWindow.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

PlaybackControlsWindow::PlaybackControlsWindow(UiManager* manager) : m_uiManager(manager) {}

void PlaybackControlsWindow::draw() {
    ImGui::Begin("Playback control");
    
    PlaybackManager* playbackManager = ServiceManager::getPlaybackManager();
    if (!playbackManager) {
        ImGui::Text("PlaybackManager not available.");
        ImGui::End();
        return;
    }
    
    size_t totalRows = playbackManager->getTotalRows();
    if (totalRows == 0) {
        ImGui::Text("No data loaded.");
        ImGui::End();
        return;
    }
    
    if (m_uiManager && m_uiManager->font_label) {
        ImGui::PushFont(m_uiManager->font_label);
    }
    ImGui::Text("Temporal index");
    if (m_uiManager && m_uiManager->font_label) {
        ImGui::PopFont();
    }
    ImGui::Separator();

    // The slider always starts from the actual PlaybackManager position
    m_sliderPosition = static_cast<int>(playbackManager->getCurrentIndex());

    // ---- Full-width slider, "index / total" label ----
    std::string label = std::to_string(m_sliderPosition + 1) + " / " + std::to_string(totalRows);
    ImGui::PushItemWidth(-FLT_MIN); // -FLT_MIN = aligned to the right edge
    bool slider_activated = ImGui::SliderInt("##TimelineSlider", &m_sliderPosition, 0, totalRows > 0 ? totalRows - 1 : 0, label.c_str());
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Timestamp of the slider position
    std::string timeStr = "--.---";
    if (auto rowOpt = playbackManager->getRowAtIndex(m_sliderPosition)) {
        if (rowOpt->count("timestamp")) {
            timeStr = rowOpt->at("timestamp");
        }
    }

    // ---- Centered line: [<]  Timestamp: x s  [>] ----
    float arrowButtonWidth = ImGui::GetFrameHeight(); 
    float spacingWidth = ImGui::GetStyle().ItemSpacing.x;

    if (m_uiManager && m_uiManager->font_data) ImGui::PushFont(m_uiManager->font_data);
    const std::string timeLabel = "Timestamp: " + timeStr + " s"; 
    float textWidth = ImGui::CalcTextSize(timeLabel.c_str()).x;
    if (m_uiManager && m_uiManager->font_data) ImGui::PopFont();

    float totalControlsWidth = arrowButtonWidth + spacingWidth + textWidth + spacingWidth + arrowButtonWidth;
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float offsetX = (windowWidth - totalControlsWidth) * 0.5f;
    if (offsetX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    bool index_changed_by_buttons = false;
    
    // Back arrow: one row (100 ms) back
    if (ImGui::ArrowButton("##prev_frame", ImGuiDir_Left)) {
        if (m_sliderPosition > 0) {
            m_sliderPosition--;
            index_changed_by_buttons = true;
        }
    }

    ImGui::SameLine();

    ImGui::BeginGroup();
    if (m_uiManager && m_uiManager->font_data) ImGui::PushFont(m_uiManager->font_data);
    ImGui::TextUnformatted(timeLabel.c_str()); 
    if (m_uiManager && m_uiManager->font_data) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine();

    // Forward arrow: one row (100 ms) forward
    if (ImGui::ArrowButton("##next_frame", ImGuiDir_Right)) {
        if (m_sliderPosition < static_cast<int>(totalRows) - 1) {
            m_sliderPosition++;
            index_changed_by_buttons = true;
        }
    }

    // Apply the new position: every window will read it on the next frame
    if (slider_activated || index_changed_by_buttons) {
        playbackManager->setCurrentIndex(static_cast<size_t>(m_sliderPosition));
    }

    ImGui::End();
}