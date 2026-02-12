/**
 * LOTRO Launcher - Character Extractor
 * 
 * Extracts character data from LOTRO client process memory.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "LotroMemoryConfig.hpp"
#include "ProcessMemory.hpp"
#include "dat/DataFacade.hpp"

#include <QString>

#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace lotro {

/**
 * Basic character information extracted from memory
 */
struct CharacterInfo {
    QString name;
    QString surname;
    QString className;
    QString race;
    QString server;
    QString account;
    int level = 0;
    
    // Stats
    int morale = 0;        // HP
    int power = 0;         // Mana
    int maxMorale = 0;
    int maxPower = 0;
    
    // Currency
    int gold = 0;
    int silver = 0;
    int copper = 0;
    int destinyPoints = 0;
    int lotroPoints = 0;
    
    // Account type
    enum class AccountType {
        Unknown,
        FreeToPlay,
        Premium,
        VIP,
        Lifetime
    };
    AccountType accountType = AccountType::Unknown;
    
    /**
     * Get total money in copper
     */
    int totalCopper() const {
        return copper + silver * 100 + gold * 100000;
    }
    
    /**
     * Get formatted money string
     */
    QString formattedMoney() const {
        return QString("%1g %2s %3c").arg(gold).arg(silver).arg(copper);
    }
    
    /**
     * Check if character data is valid
     */
    bool isValid() const {
        return !name.isEmpty() && level > 0;
    }
};

/**
 * Single virtue status
 */
struct VirtueStatus {
    QString key;       // e.g., "CHARITY", "COMPASSION"
    QString name;      // Display name
    int rank = 0;      // Current rank (0-80+)
    int xp = 0;        // Current XP
};

/**
 * Faction standing
 */
struct FactionStatus {
    int factionId = 0;
    QString key;       // e.g., "BREE", "ELVES"
    QString name;      // Display name
    QString category;  // e.g., "Eriador", "Rhovanion"
    int tier = 0;      // Current tier (1-7 typically: Enemy to Kindred)
    int reputation = 0; // Total earned reputation
};

/**
 * Crafting profession status
 */
struct CraftingProfessionStatus {
    QString name;
    int tier = 0;         // Current tier (1-15)
    int proficiency = 0;  // Proficiency points in current tier
    int mastery = 0;      // Mastery points in current tier
    bool hasMastered = false;
};

/**
 * Crafting vocation status
 */
struct CraftingStatus {
    QString vocation;
    std::vector<CraftingProfessionStatus> professions;
};

/**
 * Extended character data with all extractable elements
 */
struct CharacterData {
    CharacterInfo basic;
    
    // Virtues (20 virtues)
    std::vector<VirtueStatus> virtues;
    
    // Reputation
    std::vector<FactionStatus> factions;
    
    // Crafting
    CraftingStatus crafting;
    
    // Equipped gear slot -> item ID
    std::map<QString, int> equippedGear;
    
    // Wallet currencies (currency ID -> amount)
    std::map<int, int> wallet;
    
    // Known skills (skill IDs)
    std::vector<int> skills;
    
    // Acquired titles (title IDs)
    std::vector<int> titles;
    
    // Known emotes (emote IDs)
    std::vector<int> emotes;
    
    // Trait points spent (spec ID -> points)
    std::map<int, int> traitPoints;
};

/**
 * Character data extractor
 * 
 * Reads character information from LOTRO client process memory.
 */
class CharacterExtractor {
public:
    /**
     * Construct extractor with game installation path
     * @param gamePath Path to LOTRO installation directory (for DAT files)
     */
    explicit CharacterExtractor(const QString& gamePath = QString());
    ~CharacterExtractor();
    
    CharacterExtractor(const CharacterExtractor&) = delete;
    CharacterExtractor& operator=(const CharacterExtractor&) = delete;
    
    /**
     * Connect to LOTRO client process
     * @return true if connection successful
     */
    bool connect();
    
    /**
     * Disconnect from LOTRO client
     */
    void disconnect();
    
    /**
     * Check if connected to LOTRO client
     */
    bool isConnected() const;
    
    /**
     * Get current character info
     * @return Character info or nullopt if extraction failed
     */
    std::optional<CharacterInfo> extractCharacter();
    
    /**
     * Extract full character data including virtues, reputation, crafting
     * @return Complete character data or nullopt if extraction failed
     */
    std::optional<CharacterData> extractFullData();
    
    /**
     * Get server name
     */
    std::optional<QString> getServerName();
    
    /**
     * Get account name
     */
    std::optional<QString> getAccountName();
    
    /**
     * Get last error message
     */
    const QString& lastError() const { return m_lastError; }
    
private:
    // Read a raw property value (can be int or pointer) from an entity
    std::optional<uint64_t> readPropertyValue(uint64_t entityAddress, uint32_t propId);
    
    // Typed property readers using property ID
    std::optional<QString> readStringProperty(uint64_t entityAddress, uint32_t propId);
    std::optional<int> readIntProperty(uint64_t entityAddress, uint32_t propId);
    std::optional<int64_t> readLongProperty(uint64_t entityAddress, uint32_t propId);
    std::optional<float> readFloatProperty(uint64_t entityAddress, uint32_t propId);
    
    // Find player entity in entities table
    std::optional<uint64_t> findPlayerEntity();
    
    // Read client data structure
    bool readClientData();
    
    // Scan for memory patterns
    bool scanPatterns();
    
    // Debug: Dump all properties of an entity to find string values
    void debugDumpProperties(uint64_t entityAddress);
    
    std::unique_ptr<ProcessMemory> m_memory;
    LotroMemoryConfig m_config;
    QString m_lastError;
    
    // Cached data
    QString m_server;
    QString m_account;
    
    // DAT file access for property resolution
    QString m_gamePath;
    std::unique_ptr<dat::DataFacade> m_datFacade;
    
    // Cached property IDs (resolved from DAT files)
    int m_namePropertyId = -1;
    int m_levelPropertyId = -1;
    int m_accountNamePropertyId = -1;
    int m_subscriptionKeyPropertyId = -1;
    int m_accountTypePropertyId = -1;
    
    // Class/Race
    int m_classPropertyId = -1;
    int m_racePropertyId = -1;
    
    // Vitals
    int m_maxMoralePropertyId = -1;
    int m_currentMoralePropertyId = -1;
    int m_maxPowerPropertyId = -1;
    int m_currentPowerPropertyId = -1;
    
    // Currency
    int m_moneyPropertyId = -1;
    int m_destinyPointsPropertyId = -1;
    int m_lotroPointsPropertyId = -1;
    
    // Internal helper to read value from a generic properties hashtable
    std::optional<uint64_t> readHashtableValue(uint64_t hashtableBaseAddr, uint32_t propId);
    
    // Helper to read account property
    std::optional<uint64_t> readAccountPropertyValue(uint32_t propId);
    std::optional<QString> readAccountStringProperty(uint32_t propId);
    std::optional<int> readAccountIntProperty(uint32_t propId);
};

} // namespace lotro
