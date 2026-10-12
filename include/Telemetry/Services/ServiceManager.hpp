/**
 * @file ServiceManager.hpp
 * @brief Global (static) access point to all the backend services.
 *
 * @details
 * ServiceManager owns the services and wires them together. It is a class with
 * static members only: the UI can reach it from anywhere without passing
 * pointers around (e.g. `ServiceManager::getPlaybackManager()`).
 *
 * Owned objects and their connections:
 * @code
 *  SerialService ──> DataAggregator ──(callback)──> DataManager ──> UI windows (10 Hz)
 *                                                              └──> CsvWriter  (200 Hz, only while logging)
 *  FileService ──(loaded dataset)──> UiManager ──> PlaybackManager ──> UI windows
 *  SettingsManager, IAssetManager: user settings and resources (fonts/images)
 * @endcode
 *
 * Life cycle:
 *  - main() calls initialize() at startup and cleanup() on exit;
 *  - the UI chooses the mode with setAcquisitionMethod(), configures with
 *    configureSerial()/configureFile() and starts with startServices();
 *  - the "Home" button calls resetForNewSession(), which recreates the services.
 */
#pragma once
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include "SerialService.hpp"
#include "NetworkService.hpp"
#include "Telemetry/data_writing/DataManager.hpp"
#include "Telemetry/data_writing/CsvWriter.hpp"
#include "Telemetry/data_writing/DataAggregator.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"
#include "Telemetry/Services/FileService.hpp"
#include "../../application_services/SettingsManager.hpp"
#include "../../utils/IAssetManager.hpp"

/**
 * @brief Data source of the current session.
 */
typedef enum {
    ACQUISITION_METHOD_SERIAL = 0,   ///< Live from the serial port (SerialService).
    ACQUISITION_METHOD_NETWORK = 1,  ///< Live from the network (NetworkService, not implemented).
    ACQUISITION_METHOD_FILE = 2      ///< Playback of a .bin/.csv file (FileService + PlaybackManager).
} AcquisitionMethod;

/**
 * @brief Static registry of the services: creation, configuration, start, logging and reset.
 */
class ServiceManager {
public:
    /** @brief Creates all the services and links DataAggregator -> DataManager. Call once at startup. */
    static void initialize();
    /**
     * @brief Lists the available serial ports.
     * @return Port names (see SerialDataSource::getAvailableResources()).
     */
    static std::vector<std::string> getAllConnectionOptions();
    
    /** @brief Dispatcher of the live rows to UI and loggers. */
    static DataManager* getDataManager();
    /** @brief Manager of the session loaded from file (playback). */
    static PlaybackManager* getPlaybackManager();
    /** @brief .bin/.csv file loading service. */
    static FileService* getFileService();
    
    /**
     * @brief Sets which service startServices() will start.
     * @param method Acquisition method.
     */
    static void setAcquisitionMethod(AcquisitionMethod method);
    /**
     * @brief Configures the serial connection.
     * @param port     Port name.
     * @param baudrate Speed in bit/s.
     * @return true if the configuration was accepted.
     */
    static bool configureSerial(const std::string& port, int baudrate);
    /**
     * @brief Configures the network connection (stub).
     * @param ip   IP address.
     * @param port Port.
     * @return true if the configuration was accepted.
     */
    static bool configureNetwork(const std::string& ip, int port);
    /**
     * @brief Configures the file to load.
     * @param filePath Path of the .bin or .csv file.
     * @return true if the configuration was accepted.
     */
    static bool configureFile(const std::string& filePath);
    
    /**
     * @brief Starts the service matching the selected method.
     *
     * For files, loading starts in the background: the UI then polls
     * FileService::isFinished() every frame.
     *
     * @return true if the start succeeded.
     */
    static bool startServices();
    /** @brief Stops the running live services (serial, network). */
    static void stopTasks();
    /** @brief Stops everything and destroys all the services. Call once on exit. */
    static void cleanup();
    
    /**
     * @brief Starts CSV recording of the live session.
     *
     * In file mode it does nothing (export is done by PlaybackManager).
     *
     * @param outputDirectory Folder in which to create the `telemetry_YYYYMMDD_hhmmss.csv` file.
     * @return true if logging is active.
     */
    static bool startLogging(const std::string& outputDirectory);
    /** @brief Stops CSV recording and closes the file. */
    static void stopLogging();
    /** @brief true if a CsvWriter is active. */
    static bool isLogging();
    /**
     * @brief Brings the backend back to its initial state ("Home" button).
     *
     * Stops services and logging, cancels any running export, clears the
     * playback, resets the aggregator and recreates SerialService, FileService
     * and NetworkService.
     */
    static void resetForNewSession();
    
    /** @brief Persistent user settings (fonts, theme, ...). */
    static std::shared_ptr<SettingsManager> getSettingsManager();
    /** @brief Resource manager (fonts, images, default ImGui layout); created on first use if missing. */
    static std::shared_ptr<IAssetManager> getAssetManager();

private:
    static AcquisitionMethod m_method;                         ///< Method of the current session.
    static std::shared_ptr<IAssetManager> m_assetManager;      ///< OS-specific resources.
    static std::shared_ptr<SettingsManager> m_settingsManager; ///< User settings.
    
    // Main services
    static std::unique_ptr<SerialService> m_serialService;     ///< Serial acquisition.
    static std::unique_ptr<NetworkService> m_networkService;   ///< Network acquisition (stub).
    static std::unique_ptr<PlaybackManager> m_playbackManager; ///< File playback.
    static std::unique_ptr<FileService> m_fileService;         ///< File loading.
    
    // Data handling
    static std::unique_ptr<DataAggregator> m_dataAggregator;   ///< Statistics on the live rows.
    static std::unique_ptr<DataManager> m_dataManager;         ///< Distribution of the rows to the subscribers.
    static std::shared_ptr<CsvWriter> m_csvWriter;             ///< Active CSV logger (nullptr when not recording).
};