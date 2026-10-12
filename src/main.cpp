/**
 * @file main.cpp
 * @brief Application entry point and ARCHITECTURE OVERVIEW (read this first).
 *
 * @mainpage ERB Telemetry Software - developer guide
 *
 * Desktop application (C++20, ImGui + ImPlot on GLFW/OpenGL) that receives the
 * ECU telemetry over serial, shows it in real time, records it to CSV and lets
 * you reopen and analyse the recordings (.bin from the SD card or .csv).
 * A step-by-step guide is in DEVELOPER_GUIDE.md at the repository root.
 *
 * ## Folder map
 * @code
 *  include/ , src/                     (same structure: .hpp in include, .cpp in src)
 *   ├─ main.cpp                        startup and shutdown
 *   ├─ Telemetry/
 *   │   ├─ data_acquisition/           PROTOCOL and raw reading
 *   │   │    TelemetrySchema            <-- packet layouts and columns (single source of truth)
 *   │   │    PacketParser               live decoding of the byte stream
 *   │   │    SerialDataSource           serial port (Win/Linux/macOS)
 *   │   │    AmkDiagnostics             inverter diagnostic texts (GENERATED FILE)
 *   │   │    ThreadSafeQueue, IDataSource, BaudRateQueryable
 *   │   ├─ Services/                   services orchestrated by ServiceManager
 *   │   │    ServiceManager             static registry of all the services
 *   │   │    SerialService              acquisition + processing threads
 *   │   │    FileService                background .bin/.csv loading
 *   │   │    NetworkService             stub, not implemented
 *   │   ├─ data_writing/               after decoding
 *   │   │    DataAggregator             live statistics (variance, max/mean, meters)
 *   │   │    DataManager                distributes rows to UI (10 Hz) and loggers (200 Hz)
 *   │   │    CsvWriter                  on-track CSV log
 *   │   └─ file_reading/
 *   │        TelemetryDataset           in-memory table of a loaded session
 *   │        PlaybackManager            playback, plot data, CSV export
 *   ├─ UI/                             ImGui windows (see UIManager.hpp)
 *   ├─ application_services/           SettingsManager (user preferences in JSON)
 *   └─ utils/                          per-OS asset managers, paths, resources
 *  assets/        fonts, images, default ImGui layout, Windows resources (.rc)
 *  scripts/       gen_amk_diagnostics.py (regenerates AmkDiagnostics.*)
 *  tester_uart/   Python ECU simulator over serial (testing without the car)
 *  tester_bin_sd/ Python generator of test .bin files
 * @endcode
 *
 * ## Data flow
 * @code
 *  LIVE (serial)
 *  ECU --UART--> SerialDataSource --> SerialService [acquisition thread]
 *                                          | ThreadSafeQueue<bytes>
 *                                     SerialService [processing thread]
 *                                          | PacketParser   -> DbRow (1 row every 5 ms)
 *                                          | DataAggregator -> + statistics, meters
 *                                          v
 *                                     DataManager --(every row)--> CsvWriter (after "Start logging")
 *                                          └--(max 10 Hz)--> UI windows: onAggregatedDataReceived()
 *
 *  FILE (playback)
 *  .bin/.csv --> FileService [thread] --> TelemetryDataset --> PlaybackManager
 *                                                               | getCurrentRow() / getPlotData()
 *                                                               v
 *                                          UI windows (read every frame, no callbacks)
 * @endcode
 *
 * - A row (DbRow) is a `column name -> text` map. Column names are the names
 *   of the fields defined in TelemetrySchema.cpp.
 * - Packet A (0xCC 0x11) every 5 ms = one row; packet B (0xEE 0x11, battery)
 *   every 200 ms, its values are copied into every following A row.
 * - Timestamp: 5 ms ECU ticks converted to seconds.
 *
 * ## Threads
 * | Thread                       | Where                        | What it does                                 |
 * |------------------------------|------------------------------|----------------------------------------------|
 * | main (UI)                    | UiManager::run()             | events, ImGui drawing, state machine         |
 * | serial acquisition           | SerialService::acquisitionLoop | reads bytes from the port                  |
 * | serial processing            | SerialService::processingLoop  | parsing, statistics, CSV, UI callbacks     |
 * | file loading                 | FileService::loadingLoop     | decodes .bin/.csv                            |
 * | CSV export                   | PlaybackManager::generateCsvFromBin | high-resolution CSV writing           |
 *
 * The windows' `onAggregatedDataReceived()` callbacks run on the processing
 * thread: always protect shared data with the window mutex.
 *
 * ## Quick guides
 * - **Add / remove / change a packet value (backend)**:
 *   see TelemetrySchema.hpp ("How to ADD a sensor" section).
 * - **Show a value in a window or create a new window (frontend)**:
 *   see UIManager.hpp ("How to add a window" section).
 * - **Add statistics to a channel**: TelemetrySchema::varianceChannels()
 *   or TelemetrySchema::maxMeanChannels().
 * - **Add a new text format (enum/bitmask)**: ValueFormat in
 *   TelemetrySchema.hpp + tables and conversions in TelemetrySchema.cpp.
 * - **New persistent user setting**: SettingsManager.
 * - **New image or font**: see IAssetManager.hpp (it must be registered for every OS).
 *
 * ## Files created at runtime
 * - `output_data/` next to the executable: live CSV logs and exports (see fsutils::getExecutableDirectory()).
 * - Settings and window layout in `%APPDATA%/telemetry_manager` (Windows),
 *   `~/Library/Application Support/telemetry_manager` (macOS), `~/.config/telemetry_manager` (Linux).
 *
 * ## Logs
 * `std::cout`/`std::cerr` are redirected to the "Log terminal" window
 * (LogTerminal): lines starting with "ERROR", "WARNING" or "LOGGING" are coloured.
 * Message convention: `LEVEL [ClassName]: text.`
 */
#include <iostream>
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"

/**
 * @brief Starts the services, runs the UI loop until the window is closed, then releases everything.
 * @return 0 on normal exit.
 */
int main() {

    std::cout << "INFO [Main]: Starting up." << std::endl;    
    // Services must be created BEFORE the UI: UiManager immediately reads settings and assets
    ServiceManager::initialize();

    {
        // Dedicated scope: UiManager is destroyed (windows unsubscribed, OpenGL
        // context closed) before cleanup() releases the services
        UiManager uiManager;
        uiManager.run();
    } 
    
    ServiceManager::cleanup();
    std::cout << "INFO [Main]: Shutting down." << std::endl;

    return 0;
}