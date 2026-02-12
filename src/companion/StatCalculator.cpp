/**
 * LOTRO Launcher - Stat Calculator Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "StatCalculator.hpp"

namespace lotro {

CalculatedStats StatCalculator::calculate(const CharacterBuild& build) const {
    // Gather all stat totals
    std::unordered_map<StatType, int> totals;
    
    // Add base stats
    for (const auto& [type, value] : build.baseStats) {
        totals[type] += value;
    }
    
    // Add gear stats
    addGearStats(totals, build);
    
    // Add set bonuses
    addSetBonuses(totals, build);
    
    // Build result
    CalculatedStats result;
    
    // Primary stats
    result.might = totals[StatType::Might];
    result.agility = totals[StatType::Agility];
    result.vitality = totals[StatType::Vitality];
    result.will = totals[StatType::Will];
    result.fate = totals[StatType::Fate];
    
    // Resources - base + contribution from primary stats
    result.morale = totals[StatType::Morale] + (result.vitality * 5);
    result.power = totals[StatType::Power] + (result.will * 3);
    
    // Offensive
    result.physicalMastery = totals[StatType::PhysicalMastery];
    result.tacticalMastery = totals[StatType::TacticalMastery];
    result.criticalRating = totals[StatType::CriticalRating];
    result.finesse = totals[StatType::Finesse];
    
    // Add primary stat contributions to mastery
    // Different classes get different bonuses - simplified version
    result.physicalMastery += (result.might * 2) + result.agility;
    result.tacticalMastery += (result.will * 2);
    
    // Defensive
    result.armour = totals[StatType::Armour];
    result.criticalDefence = totals[StatType::CriticalDefence];
    result.physicalMitigation = totals[StatType::PhysicalMitigation];
    result.tacticalMitigation = totals[StatType::TacticalMitigation];
    result.resistance = totals[StatType::Resistance];
    result.blockRating = totals[StatType::BlockRating];
    result.parryRating = totals[StatType::ParryRating];
    result.evadeRating = totals[StatType::EvadeRating];
    
    // Healing
    result.incomingHealing = totals[StatType::IncomingHealing];
    result.outgoingHealing = totals[StatType::OutgoingHealing];
    result.lightOfEarendil = totals[StatType::LightOfEarendil];
    
    // Calculate derived percentages
    result.criticalChance = calculateCritChance(result.criticalRating, build.level);
    result.physicalMitigationPercent = calculateMitigation(result.physicalMitigation, build.level);
    result.tacticalMitigationPercent = calculateMitigation(result.tacticalMitigation, build.level);
    
    return result;
}

int StatCalculator::getStatTotal(const CharacterBuild& build, StatType type) const {
    int total = 0;
    
    // Base stats
    auto it = build.baseStats.find(type);
    if (it != build.baseStats.end()) {
        total += it->second;
    }
    
    // Gear stats
    for (const auto& [slot, item] : build.equipment) {
        total += item.getStat(type);
    }
    
    return total;
}

double StatCalculator::calculateCritChance(int rating, int level) const {
    // Simplified LOTRO crit formula
    // Real formula is more complex with caps and curves
    double levelFactor = 200.0 + (level * 20.0);
    double chance = (rating / levelFactor) * 100.0;
    
    // Cap at 25% (can be modified by virtues/traits)
    if (chance > 25.0) {
        chance = 25.0;
    }
    
    return chance;
}

double StatCalculator::calculateMitigation(int rating, int level) const {
    // Simplified mitigation formula
    // Against level-appropriate enemies
    double targetLevel = static_cast<double>(level);
    double levelFactor = 100.0 + (targetLevel * 40.0);
    double mitigation = (rating / (rating + levelFactor)) * 100.0;
    
    // Cap at 60% base (can be higher with armor type bonuses)
    if (mitigation > 60.0) {
        mitigation = 60.0;
    }
    
    return mitigation;
}

void StatCalculator::addGearStats(std::unordered_map<StatType, int>& totals,
                                  const CharacterBuild& build) const {
    for (const auto& [slot, item] : build.equipment) {
        for (const auto& stat : item.stats) {
            totals[stat.type] += stat.value;
        }
    }
}

void StatCalculator::addSetBonuses(std::unordered_map<StatType, int>& totals,
                                   const CharacterBuild& build) const {
    // Count pieces per set
    std::unordered_map<std::string, int> setPieces;
    for (const auto& [slot, item] : build.equipment) {
        if (!item.setName.isEmpty()) {
            setPieces[item.setName.toStdString()]++;
        }
    }
    
    // Apply set bonuses
    auto& itemDb = ItemDatabase::instance();
    for (const auto& [setName, count] : setPieces) {
        auto bonuses = itemDb.getSetBonuses(QString::fromStdString(setName));
        for (const auto& bonus : bonuses) {
            if (count >= bonus.piecesRequired) {
                for (const auto& stat : bonus.bonusStats) {
                    totals[stat.type] += stat.value;
                }
            }
        }
    }
}

} // namespace lotro
