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
#include <map>
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
    Mariner,
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
/**
 * Saved virtue snapshot
 */
struct SavedVirtue {
    QString key;
    QString name;
    int rank = 0;
    int xp = 0;
};

/**
 * Saved faction standing snapshot
 */
struct SavedFaction {
    int factionId = 0;
    QString key;
    QString name;
    QString category;
    int tier = 0;
    int reputation = 0;
};

/**
 * Saved crafting profession snapshot
 */
struct SavedCraftingProfession {
    QString name;
    int tier = 0;
    int proficiency = 0;
    int mastery = 0;
    bool hasMastered = false;
};

/**
 * Saved crafting status
 */
struct SavedCraftingStatus {
    QString vocation;
    std::vector<SavedCraftingProfession> professions;
};

/**
 * Character information with full companion data
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
    int morale = 0;
    int maxMorale = 0;
    int power = 0;
    int maxPower = 0;
    int gold = 0;
    int silver = 0;
    int copper = 0;
    int lotroPoints = 0;
    
    // Timestamps
    std::chrono::system_clock::time_point lastPlayed;
    std::chrono::system_clock::time_point created;
    
    // Account association
    QString accountName;
    
    // Extended data (populated from full extraction)
    std::vector<SavedVirtue> virtues;
    std::vector<SavedFaction> factions;
    SavedCraftingStatus crafting;
    std::map<QString, int> equippedGear;
    std::vector<int> titles;
    std::vector<int> emotes;
    std::vector<int> skills;
    std::map<int, int> traitPoints;
    
    bool hasExtendedData = false;  // true if virtues/factions/etc are populated
    
    QString classString() const;
    QString raceString() const;
};

/**
 * Character tracker
 * 
 * Tracks character information across sessions.
 * Data is persisted to local storage with full companion data support.
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
