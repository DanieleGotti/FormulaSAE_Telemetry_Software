/**
 * @file UIManager.hpp
 * @brief Main window, render loop, UI state machine and management of every window.
 *
 * @details
 * ## State machine (AppState)
 * @code
 *  SPLASH_SCREEN --(4 s)--> CONFIGURING --"Connect"--------------------> CONNECTED_LIVE
 *                               |  ^
 *                               |  └------------- "Home" (resetToHome) <-------------┐
 *                               └--"Load and analyze"--> LOADING_FILE --(done)--> CONNECTED_PLAYBACK
 * @endcode
 *  - CONFIGURING: serial selection, file selection and credits windows.
 *  - CONNECTED_LIVE / CONNECTED_PLAYBACK: all the data windows.
 *  - The "Home" menu goes back to CONFIGURING, resetting backend and windows.
 *
 * ## Two kinds of windows
 *  1. **Configuration windows** (SerialDeviceSelection, FileSelectionWindow,
 *     CreditsWindow): kept in the generic `m_uiElements` list.
 *  2. **Data windows** (Dashboard, AccBrkWindow, ...): dedicated members,
 *     created in transitionToConnectedState(). They inherit both UIElement
 *     (draw()) and IAggregatedDataSubscriber (live data reception).
 *
 * ## Skeleton of a data window (copy it to create a new one)
 * @code
 * void MyWindow::onAggregatedDataReceived(const DbRow& row) {   // serial thread!
 *     std::lock_guard<std::mutex> lock(m_dataMutex);
 *     m_latestData = row;                                       // copy only, no drawing
 * }
 * void MyWindow::draw() {                                       // UI thread, every frame
 *     DbRow data;
 *     if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
 *         if (auto row = ServiceManager::getPlaybackManager()->getCurrentRow()) data = *row;
 *     } else {
 *         std::lock_guard<std::mutex> lock(m_dataMutex);
 *         data = m_latestData;
 *     }
 *     ImGui::Begin("Unique title");   // the title is also the ID saved in imgui.ini
 *     // ... read data.at("field_name") and draw ...
 *     ImGui::End();
 * }
 * @endcode
 *
 * ## How to show a NEW packet value
 *  - Once added in TelemetrySchema.cpp, it already shows up in the Dashboard
 *    "Current data" table without touching the UI.
 *  - To show it in an existing window: read the key (the field name) from the
 *    DbRow in the window draw(), e.g. with the helper
 *    `printValue("Label", "field_name", data, "unit")` available in
 *    DynamicsPowertrainWindow / InverterDataWindow / ModulesBatteryWindow.
 *  - To add it to a plot: add the {key, label} pair to the PlotGraph in the
 *    window constructor AND the key to `keys_to_plot`
 *    (see AccBrkWindow, HallWindow, TemperatureWindow).
 *
 * ## How to add a NEW data window
 *  1. Create `include/UI/MyWindow.hpp` and `src/UI/MyWindow.cpp` by copying a
 *     similar window (text: DynamicsPowertrainWindow; plot: TemperatureWindow).
 *     New .cpp files in src/UI are picked up automatically by CMake (GLOB).
 *  2. In this header: `#include "MyWindow.hpp"` and a member
 *     `std::shared_ptr<MyWindow> m_myWindow;`.
 *  3. In UIManager.cpp:
 *     - transitionToConnectedState(): create it and, in the CONNECTED_LIVE
 *       branch, call `dataManager->addSubscriber(m_myWindow.get())`;
 *     - draw(): `if (m_myWindow) m_myWindow->draw();`;
 *     - resetToHome() and the destructor: `removeSubscriber(...)` and `m_myWindow.reset()`.
 *  4. (Optional) For a default position in the layout, update
 *     assets/resources/imgui.ini (used only on the first launch).
 *
 * @warning Do not rename the title passed to ImGui::Begin() without a reason: it
 *          is the identifier ImGui uses to save the window position and docking.
 */
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
#include "ModulesBatteryWindow.hpp"
#include "VarianceWindow.hpp"
#include "SpeedometerWindow.hpp"
#include "DynamicsPowertrainWindow.hpp"
#include "InverterDataWindow.hpp"
#include "TemperatureWindow.hpp"
#include "CreditsWindow.hpp"
#include "Telemetry/data_writing/IAggregatedDataSubscriber.hpp"

struct ImFont;

/**
 * @brief Current state of the interface (decides which windows are drawn).
 */
enum class AppState {
    SPLASH_SCREEN,       ///< Initial logo in a borderless window (4 seconds).
    CONFIGURING,         ///< Home screen: choose the serial port or a file.
    LOADING_FILE,        ///< FileService is loading a file in the background.
    CONNECTED_LIVE,      ///< Live data from the serial port.
    CONNECTED_PLAYBACK,  ///< Playback of a loaded file.
};

/**
 * @brief Owner of the GLFW window, of the ImGui/ImPlot context and of every application window.
 *
 * Single instance created in main(). Windows receive a pointer to UiManager
 * to read the current state (getCurrentState()), the fonts and the theme.
 */
class UiManager {
public:
    /**
     * @brief Creates the window (initially a 500x500 borderless splash), initialises
     *        ImGui/ImPlot, loads fonts, theme and layout and prepares the initial state.
     * @note Terminates the program (exit(1)) if GLFW cannot be initialised.
     */
    UiManager();
    /** @brief Unsubscribes the windows from the DataManager and destroys contexts and window. */
    ~UiManager();

    /** @brief Main loop: events, drawing and presentation, until the window is closed. */
    void run();
    /** @brief Handle of the GLFW window (as void*). */
    void* getWindowHandle();    
    /**
     * @brief Adds a generic window to the list drawn every frame.
     * @param uiElement Window (ownership is taken).
     */
    void addElement(std::unique_ptr<UIElement> uiElement);
    /**
     * @brief Schedules the removal of a generic window.
     *
     * The actual removal happens at the end of the frame (processPendingRemovals()),
     * so a window can request its own removal while it is being drawn.
     *
     * @param uiElement Window to remove (may be nullptr).
     */
    void removeElement(UIElement* uiElement);
    /** @brief Current state of the interface. */
    AppState getCurrentState() const;

    bool m_isDarkTheme = true;  ///< Active theme (read by the windows to pick their colours).
    ImFont* font_body;    ///< Normal text (Regular, bodyFontSize).
    ImFont* font_label;   ///< Section headers (Bold, labelFontSize).
    ImFont* font_data;    ///< Highlighted numeric values (Regular, dataFontSize).
    ImFont* font_title;   ///< Window titles (Bold, titleFontSize).

private:    
    /** @brief Draws the windows suited to the current state and handles the automatic transitions. */
    void draw();
    /** @brief Prepares the Home screen (splash on first launch) with the configuration windows. */
    void setupInitialState();
    /**
     * @brief Creates the data windows and switches to the connected state.
     * @param connectedState CONNECTED_LIVE (subscribes the windows to the DataManager) or
     *                       CONNECTED_PLAYBACK (creates the playback controls).
     */
    void transitionToConnectedState(AppState connectedState);
    /** @brief Draws the full-screen docking space and the menu bar (Home, Settings). */
    void showDockingSpace();
    /** @brief Performs the removals scheduled with removeElement(). */
    void processPendingRemovals();
    /** @brief Goes back Home: unsubscribes and destroys the data windows and resets the backend. */
    void resetToHome();

#if defined (WIN32)
    /** @brief Sets the window icon by reading the IDI_ICON1 icon from the executable resources. */
    void SetWindowIconFromResource();
#endif

    void* m_window;                                       ///< GLFWwindow* of the main window.
    AppState m_currentState = AppState::CONFIGURING;      ///< Current state.
    UIElement* m_serialSelectionWindow = nullptr;         ///< Serial selection window (in m_uiElements).
    UIElement* m_fileSelectionWindow = nullptr;           ///< File selection window (in m_uiElements).
    // ---- Data windows (they exist only in the CONNECTED_* states) ----
    std::shared_ptr<Dashboard> m_dashboard;                       ///< "Data management": logging/export and table of all values.
    std::shared_ptr<AccBrkWindow> m_accBrkWindow;                 ///< Accelerator and brake plots.
    std::shared_ptr<StatusWindow> m_statusWindow;                 ///< Digital inputs, FSMs, state of charge.
    std::shared_ptr<SteerWindow> m_steerWindow;                   ///< Animated steering wheel.
    std::shared_ptr<SuspensionWindow> m_suspensionWindow;         ///< Suspension potentiometer plot.
    std::shared_ptr<HallWindow> m_hallWindow;                     ///< Wheel speed plot.
    std::shared_ptr<ModulesBatteryWindow> m_modulesBatteryWindow; ///< EMMA and battery modules.
    std::shared_ptr<PlaybackControlsWindow> m_playbackControls;   ///< Playback slider (playback only).
    std::shared_ptr<VarianceWindow> m_varianceWindow;             ///< Statistics tables.
    std::shared_ptr<SpeedometerWindow> m_speedometerWindow;       ///< Speedometer.
    std::shared_ptr<DynamicsPowertrainWindow> m_dynamicsWindow;   ///< Vehicle dynamics and torque targets.
    std::shared_ptr<InverterDataWindow> m_inverterWindow;         ///< Inverters, cooling, AMK diagnostics.
    std::shared_ptr<TemperatureWindow> m_temperatureWindow;       ///< Motor/pump temperature plot.
    std::shared_ptr<IAggregatedDataSubscriber> m_tempDataSubscriber; ///< Not used.
    std::shared_ptr<LogTerminal> m_logTerminal;                   ///< Log console (always visible after the splash).
    std::vector<std::unique_ptr<UIElement>> m_uiElements;         ///< Generic windows (configuration, credits).
    std::vector<UIElement*> m_elementsToRemove;                   ///< Removals scheduled for the end of the frame.

    // ---- Splash screen ----
    bool m_isFirstBoot = true;        ///< The splash is shown only on the first setupInitialState().
    double m_splashStartTime = 0.0;   ///< Time (glfwGetTime) the splash started.
    GLuint m_splashTextureID = 0;     ///< OpenGL texture of the splash logo.
    int m_splashWidth = 0;            ///< Logo width (px).
    int m_splashHeight = 0;           ///< Logo height (px).
    /**
     * @brief Loads an image through the asset manager and uploads it into an OpenGL texture.
     * @param filename    Image name (e.g. "icon.png").
     * @param[out] out_texture ID of the created texture.
     * @param[out] out_width   Width in pixels.
     * @param[out] out_height  Height in pixels.
     * @return true if the image was loaded.
     */
    bool loadTextureFromMemory(const char* filename, GLuint* out_texture, int* out_width, int* out_height);

    /// GLSL version passed to the ImGui OpenGL3 backend (overridden with "#version 120" outside macOS).
#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
#else
    const char* glsl_version = "#version 130";
#endif

};