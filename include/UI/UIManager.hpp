#pragma once
#include <memory>
#include <vector>
#include "UIElement.hpp"
#include "Dashboard.hpp"
#include "AccBrkWindow.hpp"
#include "StatusWindow.hpp"
#include "SteerWindow.hpp"
#include "SuspensionWindow.hpp"
#include "HallWindow.hpp"
#include "PlaybackControlsWindow.hpp"
#include "LogTerminal.hpp"
#include "FileSelectionWindow.hpp"
#include "Telemetry/data_writing/IAggregatedDataSubscriber.hpp"

struct ImFont;

enum class AppState {
    CONFIGURING,
    LOADING_FILE,
    CONNECTED_LIVE,
    CONNECTED_PLAYBACK,
};

class UiManager {
public:
    UiManager();
    ~UiManager();

    void run();
    void* getWindowHandle();    
    void addElement(std::unique_ptr<UIElement> uiElement);
    void removeElement(UIElement* uiElement);
    AppState getCurrentState() const;

    bool m_isDarkTheme = true;
    ImFont* font_body;    // Testo standard
    ImFont* font_label;   // Etichette finestre
    ImFont* font_data;    // Valori numerici importanti
    ImFont* font_title;   // Titoli 

private:    
    void draw();
    void setupInitialState();
    void transitionToConnectedState(AppState connectedState);
    void showDockingSpace();
    void toggleTheme();
    void applyTheme();
    void processPendingRemovals();
    void resetToHome();

#if defined (WIN32)
    void SetWindowIconFromResource();
#endif

    void* m_window;
    AppState m_currentState = AppState::CONFIGURING;
    UIElement* m_serialSelectionWindow = nullptr;
    UIElement* m_fileSelectionWindow = nullptr;
    std::shared_ptr<Dashboard> m_dashboard;
    std::shared_ptr<AccBrkWindow> m_accBrkWindow;
    std::shared_ptr<StatusWindow> m_statusWindow;
    std::shared_ptr<SteerWindow> m_steerWindow;
    std::shared_ptr<SuspensionWindow> m_suspensionWindow;
    std::shared_ptr<HallWindow> m_hallWindow;
    std::shared_ptr<PlaybackControlsWindow> m_playbackControls;
    std::shared_ptr<IAggregatedDataSubscriber> m_tempDataSubscriber;
    std::shared_ptr<LogTerminal> m_logTerminal;
    std::vector<std::unique_ptr<UIElement>> m_uiElements;
    std::vector<UIElement*> m_elementsToRemove;
    bool m_shouldGenerateCsvForPlayback = false;

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
#else
    const char* glsl_version = "#version 130";
#endif

};