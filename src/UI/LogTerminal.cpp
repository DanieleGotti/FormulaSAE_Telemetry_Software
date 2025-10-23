#include <functional>
#include <iostream>
#include <imgui.h>
#include <sstream>
#include "UI/LogTerminal.hpp"
#include "UI/UIManager.hpp"

LogTerminal::LogTerminal(UiManager* manager) : m_uiManager(manager) {
    // Cattura i blocchi di testo quando lo stream viene flushato e li passa ad addMessage
    struct RedirectStreamBuf : public std::streambuf {
        RedirectStreamBuf(std::function<void(const std::string&)> callback)
            : m_callback(std::move(callback)) {}

    protected:
        // Aggiunge i caratteri a un buffer interno
        std::streamsize xsputn(const char* s, std::streamsize n) override {
            m_buffer.append(s, n);
            return n;
        }

        int_type overflow(int_type c) override {
            if (c != EOF) {
                m_buffer += static_cast<char>(c);
            }
            return c;
        }
        
        // Quando lo stream viene flushato (con std::endl o automaticamente) invia l'intero buffer accumulato
        int sync() override {
            if (!m_buffer.empty()) {
                m_callback(m_buffer);
                m_buffer.clear();
            }
            return 0;
        }

    private:
        std::string m_buffer;
        std::function<void(const std::string&)> m_callback;
    };

    m_originalCoutBuf = std::cout.rdbuf();
    m_originalCerrBuf = std::cerr.rdbuf();

    auto redirectCallback = [this](const std::string& str) { this->addMessage(str); };
    m_coutRedirect = std::make_unique<RedirectStreamBuf>(redirectCallback);
    m_cerrRedirect = std::make_unique<RedirectStreamBuf>(redirectCallback);

    std::cout.rdbuf(m_coutRedirect.get());
    std::cerr.rdbuf(m_cerrRedirect.get());

    std::cout << "INFO [LogTerminal]: LogTerminal inizializzato." << std::endl;
}

LogTerminal::~LogTerminal() {
    std::cout.rdbuf(m_originalCoutBuf);
    std::cerr.rdbuf(m_originalCerrBuf);
}

void LogTerminal::addMessage(const std::string& text_fragment) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_line_buffer += text_fragment;
    size_t pos = 0;
    while ((pos = m_line_buffer.find('\n')) != std::string::npos) {
        // Estrae la riga completa (fino al '\n')
        std::string line = m_line_buffer.substr(0, pos);
        // Pulisce l'eventuale '\r' per compatibilità Windows
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            m_messages.push_back(std::move(line));
        }

        m_line_buffer.erase(0, pos + 1);
    }
    const size_t MAX_LINES = 1000;
    while (m_messages.size() > MAX_LINES) {
        m_messages.erase(m_messages.begin());
    }
}

void LogTerminal::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_messages.clear();
    m_line_buffer.clear();
}

void LogTerminal::draw() {
    ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_FirstUseEver);
    ImGui::PushFont(m_uiManager->font_title);
    ImGui::Begin("Terminale di Log");
    ImGui::PopFont();

    if (ImGui::Button("Pulisci")) { clear(); }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::Separator();

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    std::vector<std::string> messages_copy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        messages_copy = m_messages;
    }

    for (const auto& message : messages_copy) {
        if (message.rfind("ERRORE", 0) == 0) {
            // Colore ERRORE
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", message.c_str());
        } else if (message.rfind("ATTENZIONE", 0) == 0) {
            // Colore ATTENZIONE
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", message.c_str());
        } else if (message.rfind("REGISTRAZIONE", 0) == 0) {
            // Colore REC
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 1.0f, 1.0f), "%s", message.c_str());
        } else {
            ImGui::TextUnformatted(message.c_str());
        }
    }

    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}