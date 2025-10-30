#include <application_services/SettingsManager.hpp>
#include <utils/fsUtils.hpp>
#include <fstream>

#define SETTINGSFILEPATH "settings/settings.json"

#define VERSION_KEY "version"
#define BODY_FONT_SIZE_KEY "bodyFontSize"
#define LABEL_FONT_SIZE_KEY "labelFontSize"
#define DATA_FONT_SIZE_KEY "dataFontSize"
#define TITLE_FONT_SIZE_KEY "titleFontSize"
#define GLOBAL_FONT_SCALE_KEY "globalFontScale"
#define DARK_MODE_KEY "darkMode"

#define DEFAULT_VERSION 1.0f
#define DEFAULT_BODYFONTSIZE 20.0f
#define DEFAULT_LABELFONTSIZE 20.0f
#define DEFAULT_DATAFONTSIZE 22.0f
#define DEFAULT_TITLEFONTSIZE 22.0f
#define DEFAULT_GLOBALFONTSCALE 1.0f
#define DEFAULT_DARKMODE true

SettingsManager::SettingsManager() {
    // Create settings directory if not present
    auto docPath = fsutils::getAppDocumentsDirectory();
    m_settingsFilePath = docPath / SETTINGSFILEPATH;

    std::filesystem::create_directories(m_settingsFilePath.parent_path());
    parseJsonSettingsFile();
}

bool SettingsManager::parseJsonSettingsFile() {
    // Check config file existance
    std::ifstream file(m_settingsFilePath);

    if(!file.is_open()) {
        // Create default config file
        jsoncons::json defaultConfig;

        defaultConfig[VERSION_KEY] = 1.0;
        defaultConfig[BODY_FONT_SIZE_KEY] = DEFAULT_BODYFONTSIZE;
        defaultConfig[LABEL_FONT_SIZE_KEY] = DEFAULT_LABELFONTSIZE;
        defaultConfig[DATA_FONT_SIZE_KEY] = DEFAULT_DATAFONTSIZE;
        defaultConfig[TITLE_FONT_SIZE_KEY] = DEFAULT_TITLEFONTSIZE;
        defaultConfig[GLOBAL_FONT_SCALE_KEY] = DEFAULT_GLOBALFONTSCALE;
        defaultConfig[DARK_MODE_KEY] = DEFAULT_DARKMODE;

        m_settings = defaultConfig;

        std::ofstream out(m_settingsFilePath);
        out << jsoncons::pretty_print(defaultConfig);
        out.close();
        file.close();
        return true;
    }

    try {
        // Read config file
        m_settings = jsoncons::json::parse(file);
        std::cout << "Settings loaded from " << m_settingsFilePath << std::endl;
        std::cout << jsoncons::pretty_print(m_settings) << std::endl;
        file.close();
        return false;
    } catch(const jsoncons::ser_error &e) {
        std::cerr << "Error while parsing json: " << e.what() << std::endl;
        file.close();
        return false;
    }
}

void SettingsManager::saveSettingsToFile() {
    std::ofstream out(m_settingsFilePath);
    out << jsoncons::pretty_print(m_settings);
    out.close();
}

float SettingsManager::getBodyFontSize() const {
    if(m_settings.contains(BODY_FONT_SIZE_KEY)) {
        return m_settings[BODY_FONT_SIZE_KEY].as<float>();
    }
    return DEFAULT_BODYFONTSIZE;
}

float SettingsManager::getLabelFontSize() const {
    if(m_settings.contains(LABEL_FONT_SIZE_KEY)) {
        return m_settings[LABEL_FONT_SIZE_KEY].as<float>();
    }
    return DEFAULT_LABELFONTSIZE;
}

float SettingsManager::getDataFontSize() const {
    if(m_settings.contains(DATA_FONT_SIZE_KEY)) {
        return m_settings[DATA_FONT_SIZE_KEY].as<float>();
    }
    return DEFAULT_DATAFONTSIZE;
}

float SettingsManager::getTitleFontSize() const {
    if(m_settings.contains(TITLE_FONT_SIZE_KEY)) {
        return m_settings[TITLE_FONT_SIZE_KEY].as<float>();
    }
    return DEFAULT_TITLEFONTSIZE;
}

float SettingsManager::getGlobalFontScale() const {
    if(m_settings.contains(GLOBAL_FONT_SCALE_KEY)) {
        return m_settings[GLOBAL_FONT_SCALE_KEY].as<float>();
    }
    return DEFAULT_GLOBALFONTSCALE;
}

float SettingsManager::getVersion() const {
    if(m_settings.contains(VERSION_KEY)) {
        return m_settings[VERSION_KEY].as<float>();
    }
    return DEFAULT_VERSION;
}

bool SettingsManager::getDarkMode() const {
    if(m_settings.contains(DARK_MODE_KEY)) {
        return m_settings[DARK_MODE_KEY].as<bool>();
    }
    return false; // Default to light mode
}

void SettingsManager::setBodyFontSize(float size) {
    m_settings[BODY_FONT_SIZE_KEY] = size;
    saveSettingsToFile();
}

void SettingsManager::setLabelFontSize(float size) {
    m_settings[LABEL_FONT_SIZE_KEY] = size;
    saveSettingsToFile();
}

void SettingsManager::setDataFontSize(float size) {
    m_settings[DATA_FONT_SIZE_KEY] = size;
    saveSettingsToFile();
}

void SettingsManager::setTitleFontSize(float size) {
    m_settings[TITLE_FONT_SIZE_KEY] = size;
    saveSettingsToFile();
}

void SettingsManager::setGlobalFontScale(float scale) {
    m_settings[GLOBAL_FONT_SCALE_KEY] = scale;
    saveSettingsToFile();
}

void SettingsManager::setVersion(float version) {
    m_settings[VERSION_KEY] = version;
    saveSettingsToFile();
}

void SettingsManager::setDarkMode(bool isDarkMode) {
    m_settings[DARK_MODE_KEY] = isDarkMode;
    saveSettingsToFile();
}
