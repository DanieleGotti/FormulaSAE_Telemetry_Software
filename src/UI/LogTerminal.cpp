/**
 * @file LogTerminal.cpp
 * @brief Implementation of the log console (see LogTerminal.hpp).
 */
#include <functional>
#include <iostream>
#include <imgui.h>
#include <sstream>
#include "UI/LogTerminal.hpp"
#include "UI/UIManager.hpp"

LogTerminal::LogTerminal(UiManager* manager) : m_uiManager(manager) {
    // Custom streambuf: accumulates the text and passes it to addMessage when the stream is flushed
    struct RedirectStreamBuf : public std::streambuf {
        RedirectStreamBuf(std::function<void(const std::string&)> callback)
            : m_callback(std::move(callback)) {}

    protected:
        // Write of a block of characters
        std::streamsize xsputn(const char* s, std::streamsize n) override {
            m_buffer.append(s, n);
            return n;
        }

        // Write of a single character
        int_type overflow(int_type c) override {
            if (c != EOF) {
                m_buffer += static_cast<char>(c);
            }
            return c;
        }
        
        // When the stream is flushed, send the whole accumulated buffer
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

    // Save the original buffers to restore them in the destructor
    m_originalCoutBuf = std::cout.rdbuf();
    m_originalCerrBuf = std::cerr.rdbuf();

    auto redirectCallback = [this](const std::string& str) { this->addMessage(str); };
    m_coutRedirect = std::make_unique<RedirectStreamBuf>(redirectCallback);
    m_cerrRedirect = std::make_unique<RedirectStreamBuf>(redirectCallback);

    std::cout.rdbuf(m_coutRedirect.get());
    std::cerr.rdbuf(m_cerrRedirect.get());

    std::cout << "INFO [LogTerminal]: LogTerminal initialized." << std::endl;
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
        // Extract the complete line (up to '\n')
        std::string line = m_line_buffer.substr(0, pos);
        // Remove a trailing '\r', if any (Windows compatibility)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            m_messages.push_back(std::move(line));
        }

        m_line_buffer.erase(0, pos + 1);
    }
    // Limit on the stored lines: the oldest ones are removed
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
    ImGui::Begin("Log terminal");
    ImGui::PopFont();

    if (ImGui::Button("Clear")) { clear(); }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::Separator();

    ImGui::BeginChild("Scrolling region", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    // Copy under the mutex, then draw without holding the lock
    std::vector<std::string> messages_copy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        messages_copy = m_messages;
    }

    // Colour based on the message prefix
    for (const auto& message : messages_copy) {
        if (message.rfind("ERROR", 0) == 0) {
            // ERROR: red
            ImGui::TextColored(ImVec4(0.85f, 0.10f, 0.10f, 1.00f), "%s", message.c_str());
        } else if (message.rfind("WARNING", 0) == 0) {
            // WARNING: orange
            ImGui::TextColored(ImVec4(1.00f, 0.50f, 0.00f, 1.00f), "%s", message.c_str());
        } else if (message.rfind("LOGGING", 0) == 0) {
            // LOGGING: purple
            ImGui::TextColored(ImVec4(0.60f, 0.2f, 0.85f, 1.00f), "%s", message.c_str());
        } else {
            ImGui::TextUnformatted(message.c_str());
        }
    }

    // Scroll to the bottom only if the user was already at the bottom
    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}