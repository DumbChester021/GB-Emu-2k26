#include "settings.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

// ── Helpers ──────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Recursively create directories (like mkdir -p).
static void mkdirp(const std::string& dir) {
    std::string acc;
    for (size_t i = 0; i < dir.size(); i++) {
        acc += dir[i];
        if (dir[i] == '/' || i == dir.size() - 1) {
            mkdir(acc.c_str(), 0755);
        }
    }
}

// ── Settings implementation ──────────────────────────────────────────

std::string Settings::defaultPath() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.config/gbemu/settings.ini";
}

bool Settings::load(const std::string& path) {
    path_ = path.empty() ? defaultPath() : path;

    std::ifstream f(path_);
    if (!f.is_open()) return false;  // missing file is OK

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key   = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (!key.empty()) {
            data_[key] = value;
        }
    }

    return true;
}

bool Settings::save(const std::string& path) const {
    std::string savePath = path.empty() ? path_ : path;
    if (savePath.empty()) savePath = defaultPath();

    // Ensure parent directory exists
    size_t slash = savePath.find_last_of('/');
    if (slash != std::string::npos) {
        mkdirp(savePath.substr(0, slash));
    }

    std::ofstream f(savePath);
    if (!f.is_open()) {
        std::fprintf(stderr, "Warning: could not write settings to %s\n", savePath.c_str());
        return false;
    }

    f << "# GB Emu 2k26 settings\n";
    for (const auto& kv : data_) {
        f << kv.first << "=" << kv.second << "\n";
    }

    return true;
}

std::string Settings::get(const std::string& key) const {
    auto it = data_.find(key);
    return (it != data_.end()) ? it->second : "";
}

void Settings::set(const std::string& key, const std::string& value) {
    data_[key] = value;
}

int Settings::getInt(const std::string& key, int defaultValue) const {
    std::string val = get(key);
    if (val.empty()) return defaultValue;
    try { return std::stoi(val); } catch (...) { return defaultValue; }
}

void Settings::setInt(const std::string& key, int value) {
    set(key, std::to_string(value));
}

float Settings::getFloat(const std::string& key, float defaultValue) const {
    std::string val = get(key);
    if (val.empty()) return defaultValue;
    try { return std::stof(val); } catch (...) { return defaultValue; }
}

void Settings::setFloat(const std::string& key, float value) {
    set(key, std::to_string(value));
}
