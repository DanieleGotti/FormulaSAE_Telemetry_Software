#pragma once
#include "UI/UIElement.hpp"
#include "Telemetry/data_writing/IWritingSubscriber.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"
#include <mutex>
#include <map>
#include <string>
#include <functional> 

class Dashboard : public UIElement, public IWritingSubscriber {
public:
    // Tipi di callback per comunicare con l'esterno (il main)
    using StartLoggingCallback = std::function<void()>;
    using StopLoggingCallback = std::function<void()>;

    Dashboard(StartLoggingCallback onStart, StopLoggingCallback onStop);
    void draw() override;
    void onDataReceived(const PacketParser& packet) override;
    bool createFile(const std::string& directoryPath) override;
    // Metodo per notificare alla Dashboard che la registrazione è iniziata/finita
    void setLoggingStatus(bool isLogging, const std::string& currentFile);

private:
    // Callback
    StartLoggingCallback m_onStartCallback;
    StopLoggingCallback m_onStopCallback;

    bool m_isLogging = false;
    std::string m_currentLogFile; 
    char m_filePathBuffer[256] = "telemetry_log.txt";
    std::map<std::string, PacketData> m_latestData;
    std::mutex m_dataMutex;
};