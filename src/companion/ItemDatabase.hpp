/**
 * LOTRO Launcher - Item Database
 * 
 * Database for gear items with stats, used by the Gear Simulator.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <filesystem>
#include <optional>
#include <vector>
#include <unordered_map>

namespace lotro {

/**
 * Equipment slot
 */
enum class EquipSlot {
    Head,
    Shoulders,
    Chest,
    Hands,
    Legs,
    Feet,
    Back,       // Cloak
    MainHand,
    OffHand,
    Ranged,
    Pocket,
    Earring,
    Necklace,
    Bracelet,
    Ring,
    ClassSlot,  // Class trait slot
    Unknown
};

/**
 * Item quality/rarity
 */
enum class ItemQuality {
    Common,      // White
    Uncommon,    // Yellow
    Rare,        // Purple
    Incomparable, // Teal/Cyan
    Epic,        // Orange/Gold
    Legendary,   // Purple glow
    Unknown
};

/**
 * Stat type for gear bonuses
 */
enum class StatType {
    // Primary stats
    Might,
    Agility,
    Vitality,
    Will,
    Fate,
    
    // Offensive
    PhysicalMastery,
    TacticalMastery,
    CriticalRating,
    Finesse,
    
    // Defensive
    Morale,
    Power,
    Armour,
    CriticalDefence,
    PhysicalMitigation,
    TacticalMitigation,
    Resistance,
    BlockRating,
    ParryRating,
    EvadeRating,
    
    // Special
    IncomingHealing,
    OutgoingHealing,
    LightOfEarendil,
    
    Unknown
};

/**
 * Stat bonus on an item
 */
struct ItemStat {
    StatType type;
    int value;
};

/**
 * Gear item
 */
struct GearItem {
    QString id;
    QString name;
    QString description;
    EquipSlot slot = EquipSlot::Unknown;
    ItemQuality quality = ItemQuality::Common;
    int itemLevel = 1;
    int requiredLevel = 1;
    QString requiredClass;  // Empty = any class
    
    std::vector<ItemStat> stats;
    QString setName;  // If part of a set
    
    /**
     * Get total stat value of a type
     */
    int getStat(StatType type) const {
        int total = 0;
        for (const auto& stat : stats) {
            if (stat.type == type) {
                total += stat.value;
            }
        }
        return total;
    }
};

/**
 * Equipment set bonus
 */
struct SetBonus {
    QString setName;
    int piecesRequired;
    std::vector<ItemStat> bonusStats;
    QString description;
};

/**
 * Item database for gear
 */
class ItemDatabase {
public:
    static ItemDatabase& instance();
    
    /**
     * Initialize from data directory
     */
    bool initialize(const std::filesystem::path& dataDir);
    
    bool isLoaded() const { return m_loaded; }
    
    // Item lookups
    std::vector<GearItem> searchItems(const QString& query) const;
    std::vector<GearItem> getItemsBySlot(EquipSlot slot) const;
    std::vector<GearItem> getItemsByQuality(ItemQuality quality) const;
    std::vector<GearItem> getItemsForClass(const QString& className) const;
    std::optional<GearItem> getItem(const QString& id) const;
    
    // Set lookups
    std::vector<SetBonus> getSetBonuses(const QString& setName) const;
    
    // Statistics
    int itemCount() const { return static_cast<int>(m_items.size()); }

private:
    ItemDatabase() = default;
    
    bool loadItemsXml(const std::filesystem::path& path);
    bool loadSetsXml(const std::filesystem::path& path);
    bool loadItems(const std::filesystem::path& path);
    bool loadSets(const std::filesystem::path& path);
    
    bool m_loaded = false;
    std::vector<GearItem> m_items;
    std::unordered_map<std::string, std::vector<SetBonus>> m_sets;
};

// Helper functions
QString slotName(EquipSlot slot);
QString qualityName(ItemQuality quality);
QString statName(StatType type);
QString qualityColor(ItemQuality quality);

EquipSlot parseSlot(const QString& str);
ItemQuality parseQuality(const QString& str);
StatType parseStat(const QString& str);

} // namespace lotro
