#pragma once
#include <memory>
#include <vector>
#include <string>
#include <streambuf>
#include <mutex>
#include "UI/UIElement.hpp"

class UiManager;

class LogTerminal : public UIElement {
public:
    explicit LogTerminal(UiManager* manager);
    ~LogTerminal() override;

    // No copia e spostamento
    LogTerminal(const LogTerminal&) = delete;
    LogTerminal& operator=(const LogTerminal&) = delete;

    void draw() override;

private:
    void addMessage(const std::string& message);
    void clear();

    UiManager* m_uiManager;

    std::vector<std::string> m_messages;
    std::string m_line_buffer; 
    std::mutex m_mutex;
    std::unique_ptr<std::streambuf> m_coutRedirect;
    std::unique_ptr<std::streambuf> m_cerrRedirect;
    std::streambuf* m_originalCoutBuf;
    std::streambuf* m_originalCerrBuf;

    bool m_autoScroll = true;
};