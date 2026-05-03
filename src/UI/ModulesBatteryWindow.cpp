#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include "UI/ModulesBatteryWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

// HELPER: Fa il parsing inverso dalla stringa salvata nel CSV e ricrea la dicitura con l'HEX
static std::string formatEmmaError(const std::string& errStr) {
    if (errStr == "EMMA_STATUS_OK") return "(0x0000) EMMA_STATUS_OK";
    if (errStr == "N/D" || errStr == "--" || errStr.empty()) return errStr;

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
    if (value < 50.0f) return ImVec4(0.1f, 0.8f, 0.1f, 1.0f);       // Verde scuro
    else if (value < 65.0f) return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);  // Arancione
    else return ImVec4(0.85f, 0.1f, 0.1f, 1.0f);                    // Rosso scuro
}

ImVec4 ModulesBatteryWindow::getVoltageColor(float value) {
    if (value >= 3.6f && value <= 4.2f) return ImVec4(0.1f, 0.8f, 0.1f, 1.0f); 
    else if (value > 3.0f && value < 4.5f) return ImVec4(1.0f, 0.5f, 0.0f, 1.0f); 
    else return ImVec4(0.85f, 0.1f, 0.1f, 1.0f); 
}

void ModulesBatteryWindow::printValue(const std::string& label, const std::string& key, const DbRow& dataMap, const std::string& unit) {
    ImGui::Text("%s:", label.c_str());

    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        ImGui::PushFont(m_uiManager->font_body); 
        float text_width = ImGui::CalcTextSize(it->second.c_str()).x;
        ImGui::SameLine(130.0f - text_width); // Offset ridotto
        ImGui::Text("%s", it->second.c_str()); // Usa colore di default del tema
        ImGui::PopFont();

        ImGui::SameLine(135.0f); // Offset unità
        ImGui::PushFont(m_uiManager->font_body);
        if (!unit.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%s]", unit.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[-]");
        }
        ImGui::PopFont();
    } else {
        ImGui::PushFont(m_uiManager->font_body);
        float text_width = ImGui::CalcTextSize("N/D").x;
        ImGui::SameLine(130.0f - text_width);
        ImGui::Text("N/D");
        ImGui::PopFont();
    }
}

// FUNZIONE SPECIALE PER L'ERRORE EMMA
void ModulesBatteryWindow::printErrorValue(const std::string& label, const DbRow& dataMap) {
    ImGui::Text("%s:", label.c_str()); 
    
    ImGui::SameLine(100.0f); // Allineato al testo sopra
    
    auto it = dataMap.find("emma_error");
    if (it != dataMap.end()) {
        std::string formattedStr = formatEmmaError(it->second);
        
        ImGui::PushFont(m_uiManager->font_body);
        if (it->second == "EMMA_STATUS_OK") {
            ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.1f, 1.0f), "%s", formattedStr.c_str()); // Verde
        } else {
            ImGui::TextColored(ImVec4(0.85f, 0.1f, 0.1f, 1.0f), "%s", formattedStr.c_str()); // Rosso
        }
        ImGui::PopFont();
    } else {
        ImGui::Text("N/D");
    }
}

void ModulesBatteryWindow::drawModuleCard(int index, const DbRow& dataMap) {
    std::string idxStr = std::to_string(index);
    std::string keyTmp1 = "Temperature1Module" + idxStr;
    std::string keyTmp2 = "Temperature2Module" + idxStr;
    std::string keyTens = "VoltageModule" + idxStr;

    ImGui::PushID(index);

    ImGui::PushFont(m_uiManager->font_body);
    ImGui::Text("Module %d", index); 
    ImGui::PopFont();
    ImGui::Separator();

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
                valStr = "NaN";
            }
        }

        // Calcola la larghezza del testo per il Valore
        ImGui::PushFont(m_uiManager->font_label);
        float valWidth = ImGui::CalcTextSize(valStr.c_str()).x;
        ImGui::PopFont();

        // Calcola la larghezza del testo per l'Unità di misura
        ImGui::PushFont(m_uiManager->font_body);
        float unitWidth = ImGui::CalcTextSize(unitStr.c_str()).x;
        ImGui::PopFont();

        float spacing = 5.0f; // Spazio tra il valore e l'unità di misura
        float totalRightWidth = valWidth + spacing + unitWidth;

        // Calcola l'offset relativo al bordo destro della cella (con un piccolo margine di 5.0f)
        float columnWidth = ImGui::GetColumnWidth();
        float offsetX = columnWidth - totalRightWidth - 5.0f;

        // Sicurezza: Evita la sovrapposizione tra la label (es. "Voltage:") e il valore
        // se l'utente stringe la finestra rendendo la colonna minuscola
        float minOffsetX = ImGui::CalcTextSize(label).x + 10.0f;
        if (offsetX < minOffsetX) {
            offsetX = minOffsetX;
        }

        // --- Stampa Valore ---
        ImGui::SameLine(offsetX);
        ImGui::PushFont(m_uiManager->font_body); 
        if (hasValidValue) {
            ImGui::TextColored(valColor, "%s", valStr.c_str()); 
        } else {
            ImGui::Text("%s", valStr.c_str());
        }
        ImGui::PopFont();

        // --- Stampa Unità di misura ---
        ImGui::SameLine(offsetX + valWidth + spacing); 
        ImGui::PushFont(m_uiManager->font_body);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", unitStr.c_str());
        ImGui::PopFont();
    };
    
    ImGui::BeginGroup();
    drawValue("Temperature1:", keyTmp1, true);
    drawValue("Temperature2:", keyTmp2, true);
    drawValue("Voltage:", keyTens, false); 
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
        // --- HEADER EMMA ---
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
        
        // --- DETTAGLIO MODULI ---
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Modules data");
        ImGui::PopFont();
        ImGui::Separator();

        float availWidth = ImGui::GetContentRegionAvail().x;
        float minModuleWidth = 160.0f; 
        int columns = static_cast<int>(availWidth / minModuleWidth);
        
        if (columns < 1) columns = 1;
        if (columns > 14) columns = 14;
        if (ImGui::BeginTable("ModulesGrid", columns, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingStretchSame)) {
            for (int i = 1; i <= 14; ++i) {
                ImGui::TableNextColumn();
                drawModuleCard(i, dataToDisplay);
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::Text("In attesa di dati.");
    }

    ImGui::End();
}