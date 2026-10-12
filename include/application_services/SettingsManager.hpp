/**
 * @file SettingsManager.hpp
 * @brief Persistent user preferences (font sizes, theme, first launch) stored as JSON.
 *
 * @details
 * The file is `settings/settings.json` inside the application data folder
 * (see fsutils::getAppDocumentsDirectory()). If it is missing it is created
 * with the default values. Every setter saves the file to disk immediately.
 *
 * To add a new setting:
 *  1. in SettingsManager.cpp add the JSON key (`#define XXX_KEY`) and the default (`#define DEFAULT_XXX`);
 *  2. add it to the default JSON in `parseJsonSettingsFile()`;
 *  3. add a getter (falling back to the default when the key is missing, so
 *     older files stay valid) and a setter here and in the .cpp.
 */
#pragma once
#include <filesystem>
#include <jsoncons/json.hpp>

/**
 * @brief Reads and writes the user settings. Single instance owned by ServiceManager.
 */
class SettingsManager {
public:
    /** @brief Creates the settings folder if needed and loads (or creates) the JSON file. */
    SettingsManager();
    
    /** @brief Size (px) of the normal text font. */
    float getBodyFontSize() const;
    /** @brief Size (px) of the label/section header font. */
    float getLabelFontSize() const;
    /** @brief Size (px) of the highlighted numeric value font. */
    float getDataFontSize() const;
    /** @brief Size (px) of the window title font. */
    float getTitleFontSize() const;
    /** @brief Global text scale factor (Settings > Appearance menu). */
    float getGlobalFontScale() const;
    /** @brief Version of the settings file format. */
    float getVersion() const;
    /** @brief true if the dark theme is active. */
    bool getDarkMode() const;
    /** @brief true on the first launch (used to load the default window layout). */
    bool getFirstAccess() const;

    /** @brief Sets and saves the text font size. */
    void setBodyFontSize(float size);
    /** @brief Sets and saves the label font size. */
    void setLabelFontSize(float size);
    /** @brief Sets and saves the data font size. */
    void setDataFontSize(float size);
    /** @brief Sets and saves the title font size. */
    void setTitleFontSize(float size);
    /** @brief Sets and saves the global text scale. */
    void setGlobalFontScale(float scale);
    /** @brief Sets and saves the format version. */
    void setVersion(float version);
    /** @brief Sets and saves the theme (true = dark). */
    void setDarkMode(bool isDarkMode);
    /** @brief Sets and saves the first launch flag. */
    void setFirstAccess(bool isFirstAccess);

private:
    /**
     * @brief Reads the JSON file; if it does not exist it is created with the default values.
     * @return true if the file was just created, false if it was read (or on a parse error).
     */
    bool parseJsonSettingsFile();
    /** @brief Writes the current settings to disk (indented JSON). */
    void saveSettingsToFile();

private:
    jsoncons::json m_settings;                 ///< Current settings in memory.
    std::filesystem::path m_settingsFilePath;  ///< Full path of settings.json.
};