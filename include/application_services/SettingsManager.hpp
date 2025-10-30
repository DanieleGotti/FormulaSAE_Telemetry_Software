#include <filesystem>
#include <jsoncons/json.hpp>


class SettingsManager {
public:
    SettingsManager();
    
    // Getters
    float getBodyFontSize() const;
    float getLabelFontSize() const;
    float getDataFontSize() const;
    float getTitleFontSize() const;
    float getGlobalFontScale() const;
    float getVersion() const;
    bool getDarkMode() const;

    // Setters
    void setBodyFontSize(float size);
    void setLabelFontSize(float size);
    void setDataFontSize(float size);
    void setTitleFontSize(float size);
    void setGlobalFontScale(float scale);
    void setVersion(float version);
    void setDarkMode(bool isDarkMode);

private:
    bool parseJsonSettingsFile();
    void saveSettingsToFile();

private:
    jsoncons::json m_settings;
    std::filesystem::path m_settingsFilePath;
};