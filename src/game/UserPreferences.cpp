/**
 * LOTRO Launcher - User Preferences Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "UserPreferences.hpp"

#include <fstream>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lotro {

UserPreferences::UserPreferences(const std::filesystem::path& path)
    : m_path(path)
{
    m_valid = load();
}

bool UserPreferences::load() {
    if (!std::filesystem::exists(m_path)) {
        spdlog::debug("UserPreferences file does not exist: {}", m_path.string());
        return false;
    }
    
    std::ifstream file(m_path);
    if (!file.is_open()) {
        spdlog::error("Failed to open UserPreferences: {}", m_path.string());
        return false;
    }
    
    m_data.clear();
    QString currentSection;
    std::string line;
    
    while (std::getline(file, line)) {
        // Trim whitespace
        QString qline = QString::fromStdString(line).trimmed();
        
        if (qline.isEmpty() || qline.startsWith(';') || qline.startsWith('#')) {
            continue; // Skip comments and empty lines
        }
        
        if (qline.startsWith('[') && qline.endsWith(']')) {
            // Section header
            currentSection = qline.mid(1, qline.length() - 2);
        } else if (!currentSection.isEmpty()) {
            // Key=Value pair
            int eqPos = qline.indexOf('=');
            if (eqPos > 0) {
                QString key = qline.left(eqPos).trimmed();
                QString value = qline.mid(eqPos + 1).trimmed();
                m_data[currentSection][key] = value;
            }
        }
    }
    
    spdlog::debug("Loaded UserPreferences with {} sections", m_data.size());
    return true;
}

bool UserPreferences::save() {
    return saveAs(m_path);
}

bool UserPreferences::saveAs(const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        spdlog::error("Failed to open file for writing: {}", path.string());
        return false;
    }
    
    for (const auto& [section, keys] : m_data) {
        file << "[" << section.toStdString() << "]\n";
        for (const auto& [key, value] : keys) {
            file << key.toStdString() << "=" << value.toStdString() << "\n";
        }
        file << "\n";
    }
    
    spdlog::debug("Saved UserPreferences to: {}", path.string());
    return true;
}

std::optional<QString> UserPreferences::get(
    const QString& section,
    const QString& key
) const {
    auto secIt = m_data.find(section);
    if (secIt == m_data.end()) {
        return std::nullopt;
    }
    
    auto keyIt = secIt->second.find(key);
    if (keyIt == secIt->second.end()) {
        return std::nullopt;
    }
    
    return keyIt->second;
}

void UserPreferences::set(
    const QString& section,
    const QString& key,
    const QString& value
) {
    m_data[section][key] = value;
}

void UserPreferences::remove(const QString& section, const QString& key) {
    auto secIt = m_data.find(section);
    if (secIt != m_data.end()) {
        secIt->second.erase(key);
    }
}

void UserPreferences::setAdapter(int adapterIndex) {
    set("Display", "Adapter", QString::number(adapterIndex));
}

void UserPreferences::setResolution(int width, int height) {
    set("Display", "FullscreenWidth", QString::number(width));
    set("Display", "FullscreenHeight", QString::number(height));
}

void UserPreferences::setFullscreen(bool fullscreen) {
    set("Display", "Fullscreen", fullscreen ? "1" : "0");
}

void UserPreferences::setGraphicsQuality(int quality) {
    set("Graphics", "Quality", QString::number(quality));
}

std::optional<QString> UserPreferences::getLastWorld() const {
    return get("General", "LastWorld");
}

void UserPreferences::setLastWorld(const QString& world) {
    set("General", "LastWorld", world);
}

std::optional<std::filesystem::path> findUserPreferences(
    const std::filesystem::path& settingsDir,
    bool is64bit
) {
    std::vector<std::filesystem::path> candidates;
    
    if (is64bit) {
        candidates.push_back(settingsDir / "UserPreferences64.ini");
        candidates.push_back(settingsDir / "UserPreferences_x64.ini");
    }
    candidates.push_back(settingsDir / "UserPreferences.ini");
    
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    
    return std::nullopt;
}

} // namespace lotro
