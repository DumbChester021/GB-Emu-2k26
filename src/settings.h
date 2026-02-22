#pragma once

#include <string>
#include <map>
#include <algorithm>

/// Lightweight key=value settings persistence (INI-style, no sections).
/// Default location: ~/.config/gbemu/settings.ini
class Settings {
public:
    /// Returns the platform default settings file path.
    static std::string defaultPath();

    /// Load settings from disk.  Missing file is silently ignored.
    bool load(const std::string& path = "");

    /// Save settings to disk, creating parent directories as needed.
    bool save(const std::string& path = "") const;

    /// String accessors.
    std::string get(const std::string& key) const;
    void set(const std::string& key, const std::string& value);

    /// Typed helpers — int.
    int getInt(const std::string& key, int defaultValue = 0) const;
    void setInt(const std::string& key, int value);

    /// Typed helpers — float.
    float getFloat(const std::string& key, float defaultValue = 0.0f) const;
    void setFloat(const std::string& key, float value);

private:
    std::map<std::string, std::string> data_;
    std::string path_;  // resolved path used by load/save
};
