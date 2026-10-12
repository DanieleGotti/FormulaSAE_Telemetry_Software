/**
 * @file LogTerminal.hpp
 * @brief "Log terminal" window: shows in the interface everything written to std::cout and std::cerr.
 *
 * @details
 * On construction it replaces the cout/cerr streambufs with a buffer that
 * accumulates the text and, on every flush (e.g. `std::endl`), passes it to the
 * window. The destructor restores the original streams.
 * Colours: lines starting with "ERROR" red, "WARNING" orange, "LOGGING" purple.
 * At most 1000 lines are kept.
 *
 * @note To make a message appear immediately, end it with `std::endl` (which flushes).
 */
#pragma once
#include <memory>
#include <vector>
#include <string>
#include <streambuf>
#include <mutex>
#include "UI/UIElement.hpp"

class UiManager;

/**
 * @brief Log console embedded in the UI.
 */
class LogTerminal : public UIElement {
public:
    /** @brief Redirects std::cout and std::cerr to the window. @param manager Owning UiManager. */
    explicit LogTerminal(UiManager* manager);
    /** @brief Restores the original std::cout and std::cerr streams. */
    ~LogTerminal() override;

    LogTerminal(const LogTerminal&) = delete;
    LogTerminal& operator=(const LogTerminal&) = delete;

    /** @brief Draws the "Clear"/"Auto-scroll" controls and the list of coloured messages. */
    void draw() override;

private:
    /**
     * @brief Appends a text fragment and splits it into complete lines (thread-safe).
     * @param message Fragment received from the stream (may contain several lines or a partial one).
     */
    void addMessage(const std::string& message);
    /** @brief Clears every message. */
    void clear();

    UiManager* m_uiManager;   ///< Owner.

    std::vector<std::string> m_messages;              ///< Complete lines (max 1000).
    std::string m_line_buffer;                        ///< Partial line waiting for its '\n'.
    std::mutex m_mutex;                               ///< Protects messages and buffer (written from several threads).
    std::unique_ptr<std::streambuf> m_coutRedirect;   ///< Buffer replacing the cout one.
    std::unique_ptr<std::streambuf> m_cerrRedirect;   ///< Buffer replacing the cerr one.
    std::streambuf* m_originalCoutBuf;                ///< Original cout buffer (restored at the end).
    std::streambuf* m_originalCerrBuf;                ///< Original cerr buffer (restored at the end).

    bool m_autoScroll = true;  ///< Automatically scroll to the last line.
};