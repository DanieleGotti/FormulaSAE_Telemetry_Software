#include <imgui.h>
#include <string>
#include <optional>
#include <algorithm> 
#include "UI/PlaybackControlsWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

PlaybackControlsWindow::PlaybackControlsWindow(UiManager* manager) : m_uiManager(manager) {}

void PlaybackControlsWindow::draw() {
    ImGui::Begin("Controlli Riproduzione");
    
    PlaybackManager* playbackManager = ServiceManager::getPlaybackManager();
    if (!playbackManager) {
        ImGui::Text("PlaybackManager non disponibile.");
        ImGui::End();
        return;
    }
    
    size_t totalRows = playbackManager->getTotalRows();
    if (totalRows == 0) {
        ImGui::Text("Nessun dato caricato.");
        ImGui::End();
        return;
    }
    
    if (m_uiManager && m_uiManager->font_label) {
        ImGui::PushFont(m_uiManager->font_label);
    }
    ImGui::Text("Indice temporale");
    if (m_uiManager && m_uiManager->font_label) {
        ImGui::PopFont();
    }
    ImGui::Separator();

    m_sliderPosition = static_cast<int>(playbackManager->getCurrentIndex());

    std::string label = std::to_string(m_sliderPosition + 1) + " / " + std::to_string(totalRows);
    ImGui::PushItemWidth(-FLT_MIN); // -FLT_MIN è l'idioma per "usa tutto lo spazio disponibile"
    bool slider_activated = ImGui::SliderInt("##TimelineSlider", &m_sliderPosition, 0, totalRows > 0 ? totalRows - 1 : 0, label.c_str());
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Recupera prima i dati del timestamp
    std::string timeStr = "--:--:--.---";
    std::string dateStr = "----/--/--";
    if (auto rowOpt = playbackManager->getRowAtIndex(m_sliderPosition)) {
        if (rowOpt->count("timestamp")) {
            const std::string& fullTimestamp = rowOpt->at("timestamp");
            size_t separatorPos = fullTimestamp.find('T');
            if (separatorPos != std::string::npos) {
                dateStr = fullTimestamp.substr(0, separatorPos);
                timeStr = fullTimestamp.substr(separatorPos + 1);
            } else {
                timeStr = fullTimestamp;
            }
        }
    }

    // 1. Calcola gli spazi
    float arrowButtonWidth = ImGui::GetFrameHeight(); 
    float spacingWidth = ImGui::GetStyle().ItemSpacing.x;
    
    if (m_uiManager && m_uiManager->font_data) ImGui::PushFont(m_uiManager->font_data);
    const std::string timeLabel = "Timestamp: " + timeStr;
    const std::string dateLabel = "Giorno: " + dateStr;
    float textWidth = std::max(ImGui::CalcTextSize(timeLabel.c_str()).x, ImGui::CalcTextSize(dateLabel.c_str()).x);
    if (m_uiManager && m_uiManager->font_data) ImGui::PopFont();

    float totalControlsWidth = arrowButtonWidth + spacingWidth + textWidth + spacingWidth + arrowButtonWidth;
    
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float offsetX = (windowWidth - totalControlsWidth) * 0.5f;
    if (offsetX > 0) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    }

    bool index_changed_by_buttons = false;
    
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
    ImGui::TextUnformatted(dateLabel.c_str());
    if (m_uiManager && m_uiManager->font_data) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine();

    if (ImGui::ArrowButton("##next_frame", ImGuiDir_Right)) {
        if (m_sliderPosition < static_cast<int>(totalRows) - 1) {
            m_sliderPosition++;
            index_changed_by_buttons = true;
        }
    }

    // Aggiorna il PlaybackManager se l'indice è cambiato
    if (slider_activated || index_changed_by_buttons) {
        playbackManager->setCurrentIndex(static_cast<size_t>(m_sliderPosition));
    }

    ImGui::End();
}