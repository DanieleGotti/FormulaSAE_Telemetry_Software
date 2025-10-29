#pragma once
#include <memory>
#include <vector>
#include "UIElement.hpp"
#include "Dashboard.hpp"
#include "AccBrkWindow.hpp"
#include "StatusWindow.hpp"

struct ImFont;

enum class AppState {
    CONFIGURING,
    CONNECTED
};

class UiManager {
public:
    UiManager();
    ~UiManager();

    void run();
    void* getWindowHandle();
    void addElement(std::unique_ptr<UIElement> uiElement);
    void removeElement(UIElement* uiElement);

    ImFont* font_body;    // Testo standard, log, pulsanti
    ImFont* font_label;   // Etichette dei dati (es. "ACC1A")
    ImFont* font_data;    // Valori numerici importanti
    ImFont* font_title;   // Titoli delle finestre, sezioni

private:    
    void draw();
    void setupInitialState();
    void showDockingSpace();
    void toggleTheme();
    void applyTheme();
    void processPendingRemovals();

    void* m_window;
    bool m_isDarkTheme = true;
    AppState m_currentState = AppState::CONFIGURING;
    UIElement* m_serialSelectionWindow = nullptr;
    std::shared_ptr<Dashboard> m_dashboard;
    std::shared_ptr<AccBrkWindow> m_accBrkWindow;
    std::shared_ptr<StatusWindow> m_statusWindow;
    std::vector<std::unique_ptr<UIElement>> m_uiElements;
    std::vector<UIElement*> m_elementsToRemove;

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
#else
    const char* glsl_version = "#version 130";
#endif

};