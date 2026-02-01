/**
 * LOTRO Launcher - Character Tracker
 * 
 * LOTRO Companion-like character tracking functionality.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QString>

namespace lotro {

/**
 * Character class enumeration
 */
enum class CharacterClass {
    Beorning,
    Brawler,
    Burglar,
    Captain,
    Champion,
    Guardian,
    Hunter,
    LoreMaster,
    Minstrel,
    RuneKeeper,
    Warden,
    Unknown
};

/**
 * Character race enumeration
 */
enum class CharacterRace {
    Dwarf,
    Elf,
    HighElf,
    Hobbit,
    Man,
    Beorning,
    StoutAxe,
    RiverHobbit,
    Unknown
};

/**
 * Character information
 */
struct Character {
    // Basic info
    QString name;
    QString server;
    CharacterClass characterClass = CharacterClass::Unknown;
    CharacterRace race = CharacterRace::Unknown;
    int level = 1;
    
    // Progress tracking
    QString currentArea;
    int virtueXP = 0;
    int destinyPoints = 0;
    
    // Timestamps
    std::chrono::system_clock::time_point lastPlayed;
    std::chrono::system_clock::time_point created;
    
    // Account association
    QString accountName;
    
    /**
     * Get display string for class
     */
    QString classString() const;
    
    /**
     * Get display string for race
     */
    QString raceString() const;
};

/**
 * Character tracker
 * 
 * Tracks character information across sessions.
 * Data is persisted to local storage.
 * 
 * Future: May integrate with LOTRO Companion-style client data import.
 */
class CharacterTracker {
public:
    /**
     * Create character tracker with the specified data directory
     */
    explicit CharacterTracker(const std::filesystem::path& dataDir);
    ~CharacterTracker();
    
    /**
     * Get all tracked characters
     */
    std::vector<Character> getCharacters() const;
    
    /**
     * Get characters for a specific server
     */
    std::vector<Character> getCharacters(const QString& server) const;
    
    /**
     * Get characters for a specific account
     */
    std::vector<Character> getCharactersByAccount(const QString& account) const;
    
    /**
     * Get a specific character
     */
    std::optional<Character> getCharacter(
        const QString& name,
        const QString& server
    ) const;
    
    /**
     * Add or update a character
     */
    void saveCharacter(const Character& character);
    
    /**
     * Remove a character
     */
    bool removeCharacter(const QString& name, const QString& server);
    
    /**
     * Update character last played time
     */
    void updateLastPlayed(
        const QString& name,
        const QString& server,
        std::chrono::system_clock::time_point time = std::chrono::system_clock::now()
    );
    
    /**
     * Import characters from LOTRO client data
     * 
     * Note: This is a placeholder for future LOTRO Companion-style
     * integration that would read data from the running client.
     * 
     * @return Number of characters imported
     */
    int importFromClient();
    
    /**
     * Export characters to JSON
     */
    QString exportToJson() const;
    
    /**
     * Import characters from JSON
     */
    int importFromJson(const QString& json);
    
private:
    bool load();
    bool save();
    
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * Parse character class from string
 */
CharacterClass parseCharacterClass(const QString& str);

/**
 * Parse character race from string
 */
CharacterRace parseCharacterRace(const QString& str);

} // namespace lotro
