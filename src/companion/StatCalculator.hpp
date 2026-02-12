/**
 * LOTRO Launcher - Stat Calculator
 * 
 * Calculates derived stats from gear and base stats.
 * Implements LOTRO stat formulas.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ItemDatabase.hpp"
#include <unordered_map>

namespace lotro {

/**
 * Character build for stat calculation
 */
struct CharacterBuild {
    int level = 150;
    QString characterClass;
    
    // Equipped gear by slot
    std::unordered_map<EquipSlot, GearItem> equipment;
    
    // Base stats (before gear)
    std::unordered_map<StatType, int> baseStats;
    
    /**
     * Equip an item
     */
    void equip(const GearItem& item) {
        equipment[item.slot] = item;
    }
    
    /**
     * Unequip a slot
     */
    void unequip(EquipSlot slot) {
        equipment.erase(slot);
    }
    
    /**
     * Clear all equipment
     */
    void clearGear() {
        equipment.clear();
    }
};

/**
 * Calculated stats result
 */
struct CalculatedStats {
    // Primary
    int might = 0;
    int agility = 0;
    int vitality = 0;
    int will = 0;
    int fate = 0;
    
    // Resources
    int morale = 0;
    int power = 0;
    
    // Offensive
    int physicalMastery = 0;
    int tacticalMastery = 0;
    int criticalRating = 0;
    int finesse = 0;
    
    // Defensive
    int armour = 0;
    int criticalDefence = 0;
    int physicalMitigation = 0;
    int tacticalMitigation = 0;
    int resistance = 0;
    int blockRating = 0;
    int parryRating = 0;
    int evadeRating = 0;
    
    // Derived percentages
    double criticalChance = 0.0;
    double physicalMitigationPercent = 0.0;
    double tacticalMitigationPercent = 0.0;
    
    // Healing
    int incomingHealing = 0;
    int outgoingHealing = 0;
    int lightOfEarendil = 0;
};

/**
 * Stat calculator
 * 
 * Calculates total stats from base + gear + set bonuses.
 * Implements LOTRO's stat formulas for derived values.
 */
class StatCalculator {
public:
    StatCalculator() = default;
    
    /**
     * Calculate all stats for a build
     */
    CalculatedStats calculate(const CharacterBuild& build) const;
    
    /**
     * Get total value of a stat type from build
     */
    int getStatTotal(const CharacterBuild& build, StatType type) const;
    
    /**
     * Calculate critical chance from rating
     */
    double calculateCritChance(int rating, int level) const;
    
    /**
     * Calculate mitigation percentage from rating
     */
    double calculateMitigation(int rating, int level) const;

private:
    void addGearStats(std::unordered_map<StatType, int>& totals, 
                     const CharacterBuild& build) const;
    void addSetBonuses(std::unordered_map<StatType, int>& totals,
                      const CharacterBuild& build) const;
};

} // namespace lotro
