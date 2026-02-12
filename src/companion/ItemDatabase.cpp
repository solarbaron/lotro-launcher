/**
 * LOTRO Launcher - Item Database Implementation
 * 
 * Parses LOTRO Companion XML data files for items and sets.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ItemDatabase.hpp"

#include <QFile>
#include <QXmlStreamReader>
#include <spdlog/spdlog.h>

namespace lotro {

// ============ Helper functions ============

QString slotName(EquipSlot slot) {
    switch (slot) {
        case EquipSlot::Head: return "Head";
        case EquipSlot::Shoulders: return "Shoulders";
        case EquipSlot::Chest: return "Chest";
        case EquipSlot::Hands: return "Hands";
        case EquipSlot::Legs: return "Legs";
        case EquipSlot::Feet: return "Feet";
        case EquipSlot::Back: return "Back";
        case EquipSlot::MainHand: return "Main-hand";
        case EquipSlot::OffHand: return "Off-hand";
        case EquipSlot::Ranged: return "Ranged";
        case EquipSlot::Pocket: return "Pocket";
        case EquipSlot::Earring: return "Earring";
        case EquipSlot::Necklace: return "Necklace";
        case EquipSlot::Bracelet: return "Bracelet";
        case EquipSlot::Ring: return "Ring";
        case EquipSlot::ClassSlot: return "Class Slot";
        default: return "Unknown";
    }
}

QString qualityName(ItemQuality quality) {
    switch (quality) {
        case ItemQuality::Common: return "Common";
        case ItemQuality::Uncommon: return "Uncommon";
        case ItemQuality::Rare: return "Rare";
        case ItemQuality::Incomparable: return "Incomparable";
        case ItemQuality::Epic: return "Epic";
        case ItemQuality::Legendary: return "Legendary";
        default: return "Unknown";
    }
}

QString qualityColor(ItemQuality quality) {
    switch (quality) {
        case ItemQuality::Common: return "#ffffff";       // White
        case ItemQuality::Uncommon: return "#ffff00";     // Yellow
        case ItemQuality::Rare: return "#a335ee";         // Purple
        case ItemQuality::Incomparable: return "#00ffff"; // Cyan
        case ItemQuality::Epic: return "#ff8000";         // Orange
        case ItemQuality::Legendary: return "#e6cc80";    // Gold
        default: return "#888888";
    }
}

QString statName(StatType type) {
    switch (type) {
        case StatType::Might: return "Might";
        case StatType::Agility: return "Agility";
        case StatType::Vitality: return "Vitality";
        case StatType::Will: return "Will";
        case StatType::Fate: return "Fate";
        case StatType::PhysicalMastery: return "Physical Mastery";
        case StatType::TacticalMastery: return "Tactical Mastery";
        case StatType::CriticalRating: return "Critical Rating";
        case StatType::Finesse: return "Finesse";
        case StatType::Morale: return "Morale";
        case StatType::Power: return "Power";
        case StatType::Armour: return "Armour";
        case StatType::CriticalDefence: return "Critical Defence";
        case StatType::PhysicalMitigation: return "Physical Mitigation";
        case StatType::TacticalMitigation: return "Tactical Mitigation";
        case StatType::Resistance: return "Resistance";
        case StatType::BlockRating: return "Block Rating";
        case StatType::ParryRating: return "Parry Rating";
        case StatType::EvadeRating: return "Evade Rating";
        case StatType::IncomingHealing: return "Incoming Healing";
        case StatType::OutgoingHealing: return "Outgoing Healing";
        case StatType::LightOfEarendil: return "Light of Eärendil";
        default: return "Unknown";
    }
}

// Parse LOTRO Companion XML slot names
EquipSlot parseSlot(const QString& str) {
    QString upper = str.toUpper();
    if (upper == "HEAD") return EquipSlot::Head;
    if (upper == "SHOULDER") return EquipSlot::Shoulders;
    if (upper == "CHEST") return EquipSlot::Chest;
    if (upper == "HAND") return EquipSlot::Hands;
    if (upper == "LEGS") return EquipSlot::Legs;
    if (upper == "FEET") return EquipSlot::Feet;
    if (upper == "BACK") return EquipSlot::Back;
    if (upper == "MAIN_HAND") return EquipSlot::MainHand;
    if (upper == "OFF_HAND") return EquipSlot::OffHand;
    if (upper == "EITHER_HAND") return EquipSlot::MainHand;  // Dual-wield weapons
    if (upper == "RANGED_ITEM") return EquipSlot::Ranged;
    if (upper == "POCKET") return EquipSlot::Pocket;
    if (upper == "EAR") return EquipSlot::Earring;
    if (upper == "NECK") return EquipSlot::Necklace;
    if (upper == "WRIST") return EquipSlot::Bracelet;
    if (upper == "FINGER") return EquipSlot::Ring;
    if (upper == "CLASS_SLOT") return EquipSlot::ClassSlot;
    return EquipSlot::Unknown;
}

ItemQuality parseQuality(const QString& str) {
    QString upper = str.toUpper();
    if (upper == "COMMON") return ItemQuality::Common;
    if (upper == "UNCOMMON") return ItemQuality::Uncommon;
    if (upper == "RARE") return ItemQuality::Rare;
    if (upper == "INCOMPARABLE") return ItemQuality::Incomparable;
    if (upper == "EPIC") return ItemQuality::Epic;
    if (upper == "LEGENDARY") return ItemQuality::Legendary;
    return ItemQuality::Unknown;
}

// Parse LOTRO Companion XML stat names
StatType parseStat(const QString& str) {
    QString upper = str.toUpper();
    if (upper == "MIGHT") return StatType::Might;
    if (upper == "AGILITY") return StatType::Agility;
    if (upper == "VITALITY") return StatType::Vitality;
    if (upper == "WILL") return StatType::Will;
    if (upper == "FATE") return StatType::Fate;
    if (upper == "PHYSICAL_MASTERY") return StatType::PhysicalMastery;
    if (upper == "TACTICAL_MASTERY") return StatType::TacticalMastery;
    if (upper == "CRITICAL_RATING") return StatType::CriticalRating;
    if (upper == "FINESSE") return StatType::Finesse;
    if (upper == "MORALE") return StatType::Morale;
    if (upper == "POWER") return StatType::Power;
    if (upper == "ARMOUR") return StatType::Armour;
    if (upper == "CRITICAL_DEFENCE") return StatType::CriticalDefence;
    if (upper == "PHYSICAL_MITIGATION") return StatType::PhysicalMitigation;
    if (upper == "TACTICAL_MITIGATION") return StatType::TacticalMitigation;
    if (upper == "RESISTANCE") return StatType::Resistance;
    if (upper == "BLOCK") return StatType::BlockRating;
    if (upper == "PARRY") return StatType::ParryRating;
    if (upper == "EVADE") return StatType::EvadeRating;
    if (upper == "INCOMING_HEALING") return StatType::IncomingHealing;
    if (upper == "OUTGOING_HEALING") return StatType::OutgoingHealing;
    return StatType::Unknown;
}

// ============ ItemDatabase ============

ItemDatabase& ItemDatabase::instance() {
    static ItemDatabase instance;
    return instance;
}

bool ItemDatabase::initialize(const std::filesystem::path& dataDir) {
    if (m_loaded) {
        return true;
    }
    
    spdlog::info("Initializing item database from: {}", dataDir.string());
    
    // Try LOTRO Companion XML format first (lore subdirectory)
    auto loreDir = dataDir / "lore";
    if (std::filesystem::exists(loreDir)) {
        spdlog::info("Found LOTRO Companion lore directory, loading XML data...");
        
        auto itemsXml = loreDir / "items.xml";
        if (std::filesystem::exists(itemsXml)) {
            if (!loadItemsXml(itemsXml)) {
                spdlog::warn("Failed to load items from {}", itemsXml.string());
            }
        }
        
        auto setsXml = loreDir / "sets.xml";
        if (std::filesystem::exists(setsXml)) {
            if (!loadSetsXml(setsXml)) {
                spdlog::warn("Failed to load sets from {}", setsXml.string());
            }
        }
    } else {
        // Fallback to JSON
        auto itemsPath = dataDir / "items.json";
        if (std::filesystem::exists(itemsPath)) {
            if (!loadItems(itemsPath)) {
                spdlog::warn("Failed to load items from {}", itemsPath.string());
            }
        }
    }
    
    m_loaded = true;
    spdlog::info("Item database loaded: {} items, {} sets", m_items.size(), m_sets.size());
    
    return true;
}

bool ItemDatabase::loadItemsXml(const std::filesystem::path& path) {
    spdlog::info("Loading items from XML: {}", path.string());
    
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Cannot open items XML file: {}", path.string());
        return false;
    }
    
    QXmlStreamReader xml(&file);
    int count = 0;
    int equipCount = 0;
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("item")) {
            QXmlStreamAttributes attrs = xml.attributes();
            
            // Only include equipment items (those with a slot)
            QString slotStr = attrs.value("slot").toString();
            if (slotStr.isEmpty()) {
                // Skip non-equipment items
                continue;
            }
            
            EquipSlot slot = parseSlot(slotStr);
            if (slot == EquipSlot::Unknown) {
                continue;
            }
            
            GearItem gear;
            gear.id = attrs.value("key").toString();
            gear.name = attrs.value("name").toString();
            gear.slot = slot;
            gear.quality = parseQuality(attrs.value("quality").toString());
            gear.itemLevel = attrs.value("level").toInt();
            gear.requiredLevel = attrs.value("minLevel").toInt();
            
            // Parse stats from nested elements
            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("item"))) {
                xml.readNext();
                
                if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("stat")) {
                    QString statName = xml.attributes().value("name").toString();
                    StatType statType = parseStat(statName);
                    
                    if (statType != StatType::Unknown) {
                        ItemStat stat;
                        stat.type = statType;
                        // For scaling stats, use the item level as an approximation
                        // (Real values would need progression tables)
                        stat.value = gear.itemLevel * 10;
                        gear.stats.push_back(stat);
                    }
                }
            }
            
            m_items.push_back(gear);
            equipCount++;
            count++;
            
            if (count % 10000 == 0) {
                spdlog::debug("Processed {} items ({} equipment)...", count, equipCount);
            }
        }
    }
    
    if (xml.hasError()) {
        spdlog::error("XML parse error in items: {}", xml.errorString().toStdString());
        return false;
    }
    
    spdlog::info("Loaded {} equipment items from XML", equipCount);
    return true;
}

bool ItemDatabase::loadSetsXml(const std::filesystem::path& path) {
    spdlog::info("Loading sets from XML: {}", path.string());
    
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Cannot open sets XML file: {}", path.string());
        return false;
    }
    
    QXmlStreamReader xml(&file);
    int count = 0;
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("set")) {
            QXmlStreamAttributes attrs = xml.attributes();
            QString setName = attrs.value("name").toString();
            // Clean up set name (remove level info)
            int newlinePos = setName.indexOf('\n');
            if (newlinePos > 0) {
                setName = setName.left(newlinePos);
            }
            
            int setLevel = attrs.value("level").toInt();
            
            // Parse bonuses
            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("set"))) {
                xml.readNext();
                
                if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("bonus")) {
                    SetBonus bonus;
                    bonus.setName = setName;
                    bonus.piecesRequired = xml.attributes().value("nbItems").toInt();
                    
                    // Parse bonus stats
                    while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("bonus"))) {
                        xml.readNext();
                        
                        if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("stat")) {
                            QString statName = xml.attributes().value("name").toString();
                            StatType statType = parseStat(statName);
                            
                            if (statType != StatType::Unknown) {
                                ItemStat stat;
                                stat.type = statType;
                                stat.value = setLevel * 5;  // Approximation
                                bonus.bonusStats.push_back(stat);
                            }
                        }
                    }
                    
                    if (!bonus.bonusStats.empty()) {
                        m_sets[setName.toStdString()].push_back(bonus);
                    }
                }
            }
            
            count++;
        }
    }
    
    if (xml.hasError()) {
        spdlog::error("XML parse error in sets: {}", xml.errorString().toStdString());
        return false;
    }
    
    spdlog::info("Loaded {} equipment sets from XML", count);
    return true;
}

bool ItemDatabase::loadItems(const std::filesystem::path& path) {
    // JSON fallback - stub
    return true;
}

bool ItemDatabase::loadSets(const std::filesystem::path& path) {
    // JSON fallback - stub
    return true;
}

std::vector<GearItem> ItemDatabase::searchItems(const QString& query) const {
    std::vector<GearItem> results;
    QString lowerQuery = query.toLower();
    
    for (const auto& item : m_items) {
        if (item.name.toLower().contains(lowerQuery) ||
            item.description.toLower().contains(lowerQuery)) {
            results.push_back(item);
        }
    }
    
    return results;
}

std::vector<GearItem> ItemDatabase::getItemsBySlot(EquipSlot slot) const {
    std::vector<GearItem> results;
    
    for (const auto& item : m_items) {
        if (item.slot == slot) {
            results.push_back(item);
        }
    }
    
    return results;
}

std::vector<GearItem> ItemDatabase::getItemsByQuality(ItemQuality quality) const {
    std::vector<GearItem> results;
    
    for (const auto& item : m_items) {
        if (item.quality == quality) {
            results.push_back(item);
        }
    }
    
    return results;
}

std::vector<GearItem> ItemDatabase::getItemsForClass(const QString& className) const {
    std::vector<GearItem> results;
    
    for (const auto& item : m_items) {
        if (item.requiredClass.isEmpty() || item.requiredClass == className) {
            results.push_back(item);
        }
    }
    
    return results;
}

std::optional<GearItem> ItemDatabase::getItem(const QString& id) const {
    for (const auto& item : m_items) {
        if (item.id == id) {
            return item;
        }
    }
    return std::nullopt;
}

std::vector<SetBonus> ItemDatabase::getSetBonuses(const QString& setName) const {
    auto it = m_sets.find(setName.toStdString());
    if (it != m_sets.end()) {
        return it->second;
    }
    return {};
}

} // namespace lotro
