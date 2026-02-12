/**
 * LOTRO Launcher - Game Database
 * 
 * Database for game items, deeds, recipes, titles, etc.
 * Similar to LOTRO Companion's data lookups.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QString>

namespace lotro {

/**
 * Deed category
 */
enum class DeedCategory {
    Class,
    Race,
    Social,
    Exploration,
    Quest,
    Reputation,
    Slayer,
    Lore,
    Unknown
};

/**
 * Deed information
 */
struct Deed {
    QString id;
    QString name;
    QString description;
    DeedCategory category = DeedCategory::Unknown;
    QString region;
    int level = 0;
    
    // Rewards
    int virtueXP = 0;
    int lotroPoints = 0;
    QString titleReward;
    QString traitReward;
};

/**
 * Recipe information
 */
struct Recipe {
    QString id;
    QString name;
    QString profession;     // Jeweller, Cook, etc.
    int tier = 1;
    QString category;       // Food, Jewelry, etc.
    
    struct Ingredient {
        QString itemId;
        QString name;
        int quantity;
    };
    std::vector<Ingredient> ingredients;
    
    QString outputItemId;
    QString outputItemName;
    int outputQuantity = 1;
};

/**
 * Title information
 */
struct Title {
    QString id;
    QString name;
    QString description;
    QString source;         // Quest, Deed, etc.
};

/**
 * Emote information
 */
struct Emote {
    QString id;
    QString name;
    QString command;
    QString description;
    QString source;
};

/**
 * Skill information
 */
struct Skill {
    QString id;
    QString name;
    QString category;
    int iconId = 0;
};

/**
 * Trait information  
 */
struct Trait {
    QString id;
    QString name;
    QString category;
    int iconId = 0;
    int minLevel = 0;
    bool cosmetic = false;
};

/**
 * Quest information
 */
struct Quest {
    QString id;
    QString name;
    QString category;
    int level = 0;
    QString questArc;
};

/**
 * Collection item (mounts, pets, etc.)
 */
struct CollectionItem {
    QString id;
    QString name;
    QString collectionName;
    QString category;
};

/**
 * Cosmetic item
 */
struct Cosmetic {
    QString id;
    QString name;
    QString category;
    int iconId = 0;
};

/**
 * Game database
 * 
 * Provides lookup for game data similar to LOTRO Companion.
 * Data is loaded from bundled JSON/XML files.
 */
class GameDatabase {
public:
    /**
     * Get the singleton instance
     */
    static GameDatabase& instance();
    
    /**
     * Initialize the database from the data directory
     */
    bool initialize(const std::filesystem::path& dataDir);
    
    /**
     * Check if database is loaded
     */
    bool isLoaded() const { return m_loaded; }
    
    // =================
    // Deed lookups
    // =================
    
    std::vector<Deed> searchDeeds(const QString& query) const;
    std::vector<Deed> getDeedsByCategory(DeedCategory category) const;
    std::vector<Deed> getDeedsByRegion(const QString& region) const;
    std::optional<Deed> getDeed(const QString& id) const;
    
    // =================
    // Recipe lookups
    // =================
    
    std::vector<Recipe> searchRecipes(const QString& query) const;
    std::vector<Recipe> getRecipesByProfession(const QString& profession) const;
    std::vector<Recipe> getRecipesForItem(const QString& outputItemId) const;
    std::optional<Recipe> getRecipe(const QString& id) const;
    
    // =================
    // Title lookups
    // =================
    
    std::vector<Title> searchTitles(const QString& query) const;
    std::optional<Title> getTitle(const QString& id) const;
    
    // =================
    // Emote lookups
    // =================
    
    std::vector<Emote> getAllEmotes() const;
    std::optional<Emote> getEmote(const QString& id) const;
    
    // =================
    // Skill lookups
    // =================
    
    std::vector<Skill> searchSkills(const QString& query) const;
    std::optional<Skill> getSkill(const QString& id) const;
    
    // =================
    // Trait lookups
    // =================
    
    std::vector<Trait> searchTraits(const QString& query) const;
    std::optional<Trait> getTrait(const QString& id) const;
    
    // =================
    // Statistics
    // =================
    
    int deedCount() const;
    int recipeCount() const;
    int titleCount() const;
    int emoteCount() const;
    int skillCount() const;
    int traitCount() const;
    int questCount() const;
    int collectionCount() const;
    int cosmeticCount() const;
    
private:
    GameDatabase() = default;
    ~GameDatabase() = default;
    GameDatabase(const GameDatabase&) = delete;
    GameDatabase& operator=(const GameDatabase&) = delete;
    
    bool loadDeedsXml(const std::filesystem::path& path);
    bool loadRecipesXml(const std::filesystem::path& path);
    bool loadDeeds(const std::filesystem::path& path);
    bool loadRecipes(const std::filesystem::path& path);
    bool loadTitles(const std::filesystem::path& path);
    bool loadEmotes(const std::filesystem::path& path);
    bool loadSkills(const std::filesystem::path& path);
    bool loadTraits(const std::filesystem::path& path);
    bool loadQuests(const std::filesystem::path& path);
    bool loadCollections(const std::filesystem::path& path);
    bool loadCosmetics(const std::filesystem::path& path);
    
    bool m_loaded = false;
    
    std::vector<Deed> m_deeds;
    std::vector<Recipe> m_recipes;
    std::vector<Title> m_titles;
    std::vector<Emote> m_emotes;
    std::vector<Skill> m_skills;
    std::vector<Trait> m_traits;
    std::vector<Quest> m_quests;
    std::vector<CollectionItem> m_collections;
    std::vector<Cosmetic> m_cosmetics;
};

} // namespace lotro
