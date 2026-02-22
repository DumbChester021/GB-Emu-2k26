#pragma once

#include <string>
#include <map>

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

    /// Access a value (returns empty string if key absent).
    std::string get(const std::string& key) const;

    /// Set a value.
    void set(const std::string& key, const std::string& value);

private:
    std::map<std::string, std::string> data_;
    std::string path_;  // resolved path used by load/save
};
