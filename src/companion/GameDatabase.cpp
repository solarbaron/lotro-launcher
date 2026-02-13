/**
 * LOTRO Launcher - Game Database Implementation
 * 
 * Parses LOTRO Companion XML data files.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "GameDatabase.hpp"

#include <QFile>
#include <QXmlStreamReader>
#include <fstream>
#include <spdlog/spdlog.h>

namespace lotro {

GameDatabase& GameDatabase::instance() {
    static GameDatabase instance;
    return instance;
}

bool GameDatabase::initialize(const std::filesystem::path& dataDir) {
    if (m_loaded) {
        return true;
    }
    
    spdlog::info("Initializing game database from: {}", dataDir.string());
    
    bool success = true;
    
    // Try LOTRO Companion XML format first (lore subdirectory)
    auto loreDir = dataDir / "lore";
    if (std::filesystem::exists(loreDir)) {
        spdlog::info("Found LOTRO Companion lore directory, loading XML data...");
        
        auto deedsXml = loreDir / "deeds.xml";
        if (std::filesystem::exists(deedsXml)) {
            if (!loadDeedsXml(deedsXml)) {
                spdlog::warn("Failed to load deeds from {}", deedsXml.string());
                success = false;
            }
        }
        
        auto recipesXml = loreDir / "recipes.xml";
        if (std::filesystem::exists(recipesXml)) {
            if (!loadRecipesXml(recipesXml)) {
                spdlog::warn("Failed to load recipes from {}", recipesXml.string());
            }
        }
        
        // Load core data types
        loadTitles(loreDir);
        loadEmotes(loreDir);
        loadSkills(loreDir);
        loadTraits(loreDir);
        loadQuests(loreDir);
        loadCollections(loreDir);
        loadCosmetics(loreDir);
        
        // Load new data types from LOTRO Companion
        loadFactions(loreDir);
        loadLandmarks(loreDir);
        loadGeoAreas(loreDir);
        loadCrafting(loreDir);
        loadVirtues(loreDir);
        loadClasses(loreDir);
        loadRaces(loreDir);
    } else {
        spdlog::info("No lore directory found, will use JSON fallback if available");
    }
    
    m_loaded = true;
    spdlog::info("Game database loaded: {} deeds, {} recipes, {} titles, {} emotes, "
                 "{} skills, {} traits, {} quests, {} collections, {} cosmetics, "
                 "{} factions, {} landmarks, {} geoAreas, {} professions, "
                 "{} virtues, {} classes, {} races",
                 m_deeds.size(), m_recipes.size(), m_titles.size(), m_emotes.size(),
                 m_skills.size(), m_traits.size(), m_quests.size(), m_collections.size(),
                 m_cosmetics.size(), m_factions.size(), m_landmarks.size(),
                 m_geoAreas.size(), m_professions.size(), m_virtues.size(),
                 m_classes.size(), m_races.size());
    
    return success;
}

// Helper to parse deed type from LOTRO Companion format
static DeedCategory parseDeedType(const QString& type) {
    if (type == "CLASS") return DeedCategory::Class;
    if (type == "RACE") return DeedCategory::Race;
    if (type == "SOCIAL") return DeedCategory::Social;
    if (type == "EXPLORATION") return DeedCategory::Exploration;
    if (type == "QUEST") return DeedCategory::Quest;
    if (type == "REPUTATION") return DeedCategory::Reputation;
    if (type == "SLAYER") return DeedCategory::Slayer;
    if (type == "LORE") return DeedCategory::Lore;
    return DeedCategory::Unknown;
}

bool GameDatabase::loadDeedsXml(const std::filesystem::path& path) {
    spdlog::info("Loading deeds from XML: {}", path.string());
    
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Cannot open deeds XML file: {}", path.string());
        return false;
    }
    
    QXmlStreamReader xml(&file);
    int count = 0;
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("deed")) {
            Deed deed;
            QXmlStreamAttributes attrs = xml.attributes();
            
            deed.id = attrs.value("id").toString();
            deed.name = attrs.value("name").toString();
            deed.level = attrs.value("level").toInt();
            deed.category = parseDeedType(attrs.value("type").toString());
            deed.region = attrs.value("requiredClass").toString(); // Store class as region for now
            
            // Parse description from attribute if present
            if (attrs.hasAttribute("description")) {
                deed.description = attrs.value("description").toString();
            }
            
            // Parse child elements for rewards
            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("deed"))) {
                xml.readNext();
                
                if (xml.tokenType() == QXmlStreamReader::StartElement) {
                    if (xml.name() == QStringLiteral("rewards")) {
                        // Parse rewards
                        while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("rewards"))) {
                            xml.readNext();
                            
                            if (xml.tokenType() == QXmlStreamReader::StartElement) {
                                if (xml.name() == QStringLiteral("lotroPoints")) {
                                    deed.lotroPoints = xml.attributes().value("quantity").toInt();
                                } else if (xml.name() == QStringLiteral("virtueXP")) {
                                    deed.virtueXP = xml.attributes().value("quantity").toInt();
                                } else if (xml.name() == QStringLiteral("title")) {
                                    deed.titleReward = xml.attributes().value("name").toString();
                                } else if (xml.name() == QStringLiteral("trait")) {
                                    deed.traitReward = xml.attributes().value("name").toString();
                                }
                            }
                        }
                    }
                }
            }
            
            m_deeds.push_back(deed);
            count++;
            
            // Log progress every 1000 deeds
            if (count % 1000 == 0) {
                spdlog::debug("Loaded {} deeds...", count);
            }
        }
    }
    
    if (xml.hasError()) {
        spdlog::error("XML parse error in deeds: {}", xml.errorString().toStdString());
        return false;
    }
    
    spdlog::info("Loaded {} deeds from XML", count);
    return true;
}

bool GameDatabase::loadRecipesXml(const std::filesystem::path& path) {
    spdlog::info("Loading recipes from XML: {}", path.string());
    
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Cannot open recipes XML file: {}", path.string());
        return false;
    }
    
    QXmlStreamReader xml(&file);
    int count = 0;
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("recipe")) {
            Recipe recipe;
            QXmlStreamAttributes attrs = xml.attributes();
            
            recipe.id = attrs.value("id").toString();
            recipe.name = attrs.value("name").toString();
            recipe.profession = attrs.value("profession").toString();
            recipe.tier = attrs.value("tier").toInt();
            
            // LOTRO Companion XML wraps ingredients/results inside <recipeResult>
            // Structure: <recipe><recipeResult><ingredient>...</ingredient><result>...</result></recipeResult></recipe>
            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("recipe"))) {
                xml.readNext();
                
                if (xml.tokenType() == QXmlStreamReader::StartElement) {
                    // Handle both direct children and recipeResult wrapper
                    if (xml.name() == QStringLiteral("ingredient")) {
                        int quantity = xml.attributes().value("quantity").toInt();
                        if (quantity == 0) quantity = 1;
                        
                        bool isOptional = xml.attributes().value("optional").toString() == "true";
                        
                        // Get the ingredient item
                        while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("ingredient"))) {
                            xml.readNext();
                            if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("ingredientItem")) {
                                // Skip optional crafting ingredients for the basic recipe display
                                if (!isOptional) {
                                    Recipe::Ingredient ing;
                                    ing.itemId = xml.attributes().value("itemId").toString();
                                    ing.name = xml.attributes().value("name").toString();
                                    ing.quantity = quantity;
                                    recipe.ingredients.push_back(ing);
                                }
                            }
                        }
                    } else if (xml.name() == QStringLiteral("result")) {
                        bool isCritical = xml.attributes().value("critical").toString() == "true";
                        if (!isCritical) {
                            recipe.outputQuantity = xml.attributes().value("quantity").toInt();
                            if (recipe.outputQuantity == 0) recipe.outputQuantity = 1;
                            
                            // Get result item
                            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("result"))) {
                                xml.readNext();
                                if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("resultItem")) {
                                    recipe.outputItemId = xml.attributes().value("itemId").toString();
                                    recipe.outputItemName = xml.attributes().value("name").toString();
                                }
                            }
                        }
                    }
                    // recipeResult is just a wrapper — we process its children naturally
                }
            }
            
            m_recipes.push_back(recipe);
            count++;
            
            if (count % 1000 == 0) {
                spdlog::debug("Loaded {} recipes...", count);
            }
        }
    }
    
    if (xml.hasError()) {
        spdlog::error("XML parse error in recipes: {}", xml.errorString().toStdString());
        return false;
    }
    
    spdlog::info("Loaded {} recipes from XML", count);
    return true;
}

bool GameDatabase::loadTitles(const std::filesystem::path& path) {
    // Try titles.xml from LOTRO Companion format
    auto titlesPath = path / "titles.xml";
    if (!std::filesystem::exists(titlesPath)) {
        spdlog::warn("titles.xml not found at {}", titlesPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(titlesPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open titles.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QString("title")) {
                Title t;
                QXmlStreamAttributes attrs = xml.attributes();
                
                t.id = attrs.value("id").toString();
                t.name = attrs.value("name").toString();
                t.description = attrs.value("category").toString(); // Use category as description
                t.source = attrs.value("exclusionGroup").toString();
                
                if (!t.id.isEmpty() && !t.name.isEmpty()) {
                    m_titles.push_back(t);
                    count++;
                }
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} titles from XML", count);
    return true;
}

bool GameDatabase::loadEmotes(const std::filesystem::path& path) {
    // Try emotes.xml from LOTRO Companion format
    auto emotesPath = path / "emotes.xml";
    if (!std::filesystem::exists(emotesPath)) {
        spdlog::warn("emotes.xml not found at {}", emotesPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(emotesPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open emotes.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QString("emote")) {
                Emote e;
                QXmlStreamAttributes attrs = xml.attributes();
                
                e.id = attrs.value("id").toString();
                e.command = attrs.value("command").toString();
                e.name = e.command; // Use command as name
                
                // auto="true" means available to all characters
                QString autoAttr = attrs.value("auto").toString();
                e.source = autoAttr == "true" ? "Default" : "Special";
                
                if (!e.id.isEmpty() && !e.command.isEmpty()) {
                    m_emotes.push_back(e);
                    count++;
                }
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} emotes from XML", count);
    return true;
}

bool GameDatabase::loadSkills(const std::filesystem::path& path) {
    auto skillsPath = path / "skills.xml";
    if (!std::filesystem::exists(skillsPath)) {
        spdlog::warn("skills.xml not found at {}", skillsPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(skillsPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open skills.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QString("skill")) {
                Skill s;
                QXmlStreamAttributes attrs = xml.attributes();
                
                s.id = attrs.value("identifier").toString();
                s.name = attrs.value("name").toString();
                s.category = attrs.value("category").toString();
                s.iconId = attrs.value("iconId").toInt();
                
                if (!s.id.isEmpty() && !s.name.isEmpty()) {
                    m_skills.push_back(s);
                    count++;
                }
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} skills from XML", count);
    return true;
}

bool GameDatabase::loadTraits(const std::filesystem::path& path) {
    auto traitsPath = path / "traits.xml";
    if (!std::filesystem::exists(traitsPath)) {
        spdlog::warn("traits.xml not found at {}", traitsPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(traitsPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open traits.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QString("trait")) {
                Trait t;
                QXmlStreamAttributes attrs = xml.attributes();
                
                t.id = attrs.value("identifier").toString();
                t.name = attrs.value("name").toString();
                t.category = attrs.value("category").toString();
                t.iconId = attrs.value("iconId").toInt();
                t.minLevel = attrs.value("minLevel").toInt();
                t.cosmetic = attrs.value("cosmetic").toString() == "true";
                
                if (!t.id.isEmpty() && !t.name.isEmpty()) {
                    m_traits.push_back(t);
                    count++;
                }
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} traits from XML", count);
    return true;
}

bool GameDatabase::loadQuests(const std::filesystem::path& path) {
    auto questsPath = path / "quests.xml";
    if (!std::filesystem::exists(questsPath)) {
        spdlog::warn("quests.xml not found at {}", questsPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(questsPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open quests.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QString("quest")) {
                Quest q;
                QXmlStreamAttributes attrs = xml.attributes();
                
                q.id = attrs.value("id").toString();
                q.name = attrs.value("name").toString();
                q.category = attrs.value("category").toString();
                q.level = attrs.value("level").toInt();
                q.questArc = attrs.value("questArc").toString();
                
                if (!q.id.isEmpty() && !q.name.isEmpty()) {
                    m_quests.push_back(q);
                    count++;
                }
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} quests from XML", count);
    return true;
}

bool GameDatabase::loadCollections(const std::filesystem::path& path) {
    auto collectionsPath = path / "collections.xml";
    if (!std::filesystem::exists(collectionsPath)) {
        spdlog::warn("collections.xml not found at {}", collectionsPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(collectionsPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open collections.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    QString currentCollectionName;
    QString currentCategory;
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QString("collection")) {
                QXmlStreamAttributes attrs = xml.attributes();
                currentCollectionName = attrs.value("name").toString();
                currentCategory = attrs.value("category").toString();
            }
            else if (xml.name() == QString("element")) {
                CollectionItem item;
                QXmlStreamAttributes attrs = xml.attributes();
                
                item.id = attrs.value("id").toString();
                item.name = attrs.value("name").toString();
                item.collectionName = currentCollectionName;
                item.category = currentCategory;
                
                if (!item.id.isEmpty() && !item.name.isEmpty()) {
                    m_collections.push_back(item);
                    count++;
                }
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} collection items from XML", count);
    return true;
}

bool GameDatabase::loadCosmetics(const std::filesystem::path& path) {
    auto cosmeticsPath = path / "cosmetics.xml";
    if (!std::filesystem::exists(cosmeticsPath)) {
        spdlog::warn("cosmetics.xml not found at {}", cosmeticsPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(cosmeticsPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open cosmetics.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QString("cosmetic")) {
                Cosmetic c;
                QXmlStreamAttributes attrs = xml.attributes();
                
                c.id = attrs.value("id").toString();
                c.name = attrs.value("name").toString();
                c.category = attrs.value("category").toString();
                c.iconId = attrs.value("iconId").toInt();
                
                if (!c.id.isEmpty() && !c.name.isEmpty()) {
                    m_cosmetics.push_back(c);
                    count++;
                }
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} cosmetic items from XML", count);
    return true;
}

// ============ Deed Lookups ============

std::vector<Deed> GameDatabase::searchDeeds(const QString& query) const {
    std::vector<Deed> results;
    QString lowerQuery = query.toLower();
    
    for (const auto& deed : m_deeds) {
        if (deed.name.toLower().contains(lowerQuery) ||
            deed.description.toLower().contains(lowerQuery) ||
            deed.region.toLower().contains(lowerQuery)) {
            results.push_back(deed);
        }
    }
    
    return results;
}

std::vector<Deed> GameDatabase::getDeedsByCategory(DeedCategory category) const {
    std::vector<Deed> results;
    
    for (const auto& deed : m_deeds) {
        if (deed.category == category) {
            results.push_back(deed);
        }
    }
    
    return results;
}

std::vector<Deed> GameDatabase::getDeedsByRegion(const QString& region) const {
    std::vector<Deed> results;
    
    for (const auto& deed : m_deeds) {
        if (deed.region == region) {
            results.push_back(deed);
        }
    }
    
    return results;
}

std::optional<Deed> GameDatabase::getDeed(const QString& id) const {
    for (const auto& deed : m_deeds) {
        if (deed.id == id) {
            return deed;
        }
    }
    return std::nullopt;
}

// ============ Recipe Lookups ============

std::vector<Recipe> GameDatabase::searchRecipes(const QString& query) const {
    std::vector<Recipe> results;
    QString lowerQuery = query.toLower();
    
    for (const auto& recipe : m_recipes) {
        if (recipe.name.toLower().contains(lowerQuery) ||
            recipe.outputItemName.toLower().contains(lowerQuery)) {
            results.push_back(recipe);
        }
    }
    
    return results;
}

std::vector<Recipe> GameDatabase::getRecipesByProfession(const QString& profession) const {
    std::vector<Recipe> results;
    
    for (const auto& recipe : m_recipes) {
        if (recipe.profession.compare(profession, Qt::CaseInsensitive) == 0) {
            results.push_back(recipe);
        }
    }
    
    return results;
}

std::vector<Recipe> GameDatabase::getRecipesForItem(const QString& outputItemId) const {
    std::vector<Recipe> results;
    
    for (const auto& recipe : m_recipes) {
        if (recipe.outputItemId == outputItemId) {
            results.push_back(recipe);
        }
    }
    
    return results;
}

std::optional<Recipe> GameDatabase::getRecipe(const QString& id) const {
    for (const auto& recipe : m_recipes) {
        if (recipe.id == id) {
            return recipe;
        }
    }
    return std::nullopt;
}

// ============ Title Lookups ============

std::vector<Title> GameDatabase::searchTitles(const QString& query) const {
    std::vector<Title> results;
    QString lowerQuery = query.toLower();
    
    for (const auto& title : m_titles) {
        if (title.name.toLower().contains(lowerQuery) ||
            title.description.toLower().contains(lowerQuery)) {
            results.push_back(title);
        }
    }
    
    return results;
}

std::optional<Title> GameDatabase::getTitle(const QString& id) const {
    for (const auto& title : m_titles) {
        if (title.id == id) {
            return title;
        }
    }
    return std::nullopt;
}

// ============ Emote Lookups ============

std::vector<Emote> GameDatabase::getAllEmotes() const {
    return m_emotes;
}

std::optional<Emote> GameDatabase::getEmote(const QString& id) const {
    for (const auto& emote : m_emotes) {
        if (emote.id == id) {
            return emote;
        }
    }
    return std::nullopt;
}

// ============ Statistics ============

int GameDatabase::deedCount() const {
    return static_cast<int>(m_deeds.size());
}

int GameDatabase::recipeCount() const {
    return static_cast<int>(m_recipes.size());
}

int GameDatabase::titleCount() const {
    return static_cast<int>(m_titles.size());
}

int GameDatabase::emoteCount() const {
    return static_cast<int>(m_emotes.size());
}

// ============ Skill Lookups ============

std::vector<Skill> GameDatabase::searchSkills(const QString& query) const {
    std::vector<Skill> results;
    QString lowerQuery = query.toLower();
    
    for (const auto& skill : m_skills) {
        if (query.isEmpty() || skill.name.toLower().contains(lowerQuery)) {
            results.push_back(skill);
        }
    }
    return results;
}

std::optional<Skill> GameDatabase::getSkill(const QString& id) const {
    for (const auto& skill : m_skills) {
        if (skill.id == id) {
            return skill;
        }
    }
    return std::nullopt;
}

// ============ Trait Lookups ============

std::vector<Trait> GameDatabase::searchTraits(const QString& query) const {
    std::vector<Trait> results;
    QString lowerQuery = query.toLower();
    
    for (const auto& trait : m_traits) {
        if (query.isEmpty() || trait.name.toLower().contains(lowerQuery)) {
            results.push_back(trait);
        }
    }
    return results;
}

std::optional<Trait> GameDatabase::getTrait(const QString& id) const {
    for (const auto& trait : m_traits) {
        if (trait.id == id) {
            return trait;
        }
    }
    return std::nullopt;
}

int GameDatabase::skillCount() const {
    return static_cast<int>(m_skills.size());
}

int GameDatabase::traitCount() const {
    return static_cast<int>(m_traits.size());
}

int GameDatabase::questCount() const {
    return static_cast<int>(m_quests.size());
}

int GameDatabase::collectionCount() const {
    return static_cast<int>(m_collections.size());
}

int GameDatabase::cosmeticCount() const {
    return static_cast<int>(m_cosmetics.size());
}

// ============ Faction Loading & Lookups ============

bool GameDatabase::loadFactions(const std::filesystem::path& path) {
    auto factionsPath = path / "factions.xml";
    if (!std::filesystem::exists(factionsPath)) {
        spdlog::warn("factions.xml not found at {}", factionsPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(factionsPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open factions.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("faction")) {
            Faction f;
            QXmlStreamAttributes attrs = xml.attributes();
            
            f.id = attrs.value("id").toString();
            f.key = attrs.value("key").toString();
            f.name = attrs.value("name").toString();
            f.category = attrs.value("category").toString();
            f.lowestTier = attrs.value("lowestTier").toInt();
            f.initialTier = attrs.value("initialTier").toInt();
            f.highestTier = attrs.value("highestTier").toInt();
            f.currentTierProperty = attrs.value("currentTierProperty").toString();
            f.currentReputationProperty = attrs.value("currentReputationProperty").toString();
            
            // Parse tier levels
            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("faction"))) {
                xml.readNext();
                
                if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("level")) {
                    FactionTier tier;
                    QXmlStreamAttributes tierAttrs = xml.attributes();
                    tier.tier = tierAttrs.value("tier").toInt();
                    tier.key = tierAttrs.value("key").toString();
                    tier.requiredReputation = tierAttrs.value("requiredReputation").toInt();
                    tier.lotroPoints = tierAttrs.value("lotroPoints").toInt();
                    tier.deedKey = tierAttrs.value("deedKey").toString();
                    f.tiers.push_back(tier);
                }
            }
            
            if (!f.id.isEmpty() && !f.name.isEmpty()) {
                m_factions.push_back(f);
                count++;
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} factions from XML", count);
    return true;
}

std::vector<Faction> GameDatabase::getAllFactions() const {
    return m_factions;
}

std::vector<Faction> GameDatabase::getFactionsByCategory(const QString& category) const {
    std::vector<Faction> results;
    for (const auto& f : m_factions) {
        if (f.category.compare(category, Qt::CaseInsensitive) == 0) {
            results.push_back(f);
        }
    }
    return results;
}

std::optional<Faction> GameDatabase::getFaction(const QString& id) const {
    for (const auto& f : m_factions) {
        if (f.id == id) {
            return f;
        }
    }
    return std::nullopt;
}

int GameDatabase::factionCount() const {
    return static_cast<int>(m_factions.size());
}

// ============ Landmark Loading & Lookups ============

bool GameDatabase::loadLandmarks(const std::filesystem::path& path) {
    auto landmarksPath = path / "landmarks.xml";
    if (!std::filesystem::exists(landmarksPath)) {
        spdlog::warn("landmarks.xml not found at {}", landmarksPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(landmarksPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open landmarks.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("landmark")) {
            Landmark lm;
            QXmlStreamAttributes attrs = xml.attributes();
            
            lm.id = attrs.value("id").toString();
            lm.name = attrs.value("name").toString();
            
            if (!lm.id.isEmpty() && !lm.name.isEmpty()) {
                m_landmarks.push_back(lm);
                count++;
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} landmarks from XML", count);
    return true;
}

std::vector<Landmark> GameDatabase::searchLandmarks(const QString& query) const {
    std::vector<Landmark> results;
    QString lowerQuery = query.toLower();
    
    for (const auto& lm : m_landmarks) {
        if (query.isEmpty() || lm.name.toLower().contains(lowerQuery)) {
            results.push_back(lm);
        }
    }
    return results;
}

std::optional<Landmark> GameDatabase::getLandmark(const QString& id) const {
    for (const auto& lm : m_landmarks) {
        if (lm.id == id) {
            return lm;
        }
    }
    return std::nullopt;
}

int GameDatabase::landmarkCount() const {
    return static_cast<int>(m_landmarks.size());
}

// ============ GeoArea Loading & Lookups ============

bool GameDatabase::loadGeoAreas(const std::filesystem::path& path) {
    auto geoPath = path / "geoAreas.xml";
    if (!std::filesystem::exists(geoPath)) {
        spdlog::warn("geoAreas.xml not found at {}", geoPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(geoPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open geoAreas.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            GeoArea area;
            QXmlStreamAttributes attrs = xml.attributes();
            
            if (xml.name() == QStringLiteral("region")) {
                area.type = GeoAreaType::Region;
            } else if (xml.name() == QStringLiteral("territory")) {
                area.type = GeoAreaType::Territory;
            } else if (xml.name() == QStringLiteral("area")) {
                area.type = GeoAreaType::Area;
            } else if (xml.name() == QStringLiteral("dungeon")) {
                area.type = GeoAreaType::Dungeon;
            } else {
                continue;
            }
            
            area.id = attrs.value("id").toString();
            area.name = attrs.value("name").toString();
            area.parentId = attrs.value("parentId").toString();
            
            if (!area.id.isEmpty() && !area.name.isEmpty()) {
                m_geoAreas.push_back(area);
                count++;
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} geographic areas from XML", count);
    return true;
}

std::vector<GeoArea> GameDatabase::getAllRegions() const {
    std::vector<GeoArea> results;
    for (const auto& area : m_geoAreas) {
        if (area.type == GeoAreaType::Region) {
            results.push_back(area);
        }
    }
    return results;
}

std::vector<GeoArea> GameDatabase::getTerritoriesForRegion(const QString& regionId) const {
    std::vector<GeoArea> results;
    for (const auto& area : m_geoAreas) {
        if (area.type == GeoAreaType::Territory && area.parentId == regionId) {
            results.push_back(area);
        }
    }
    return results;
}

std::optional<GeoArea> GameDatabase::getGeoArea(const QString& id) const {
    for (const auto& area : m_geoAreas) {
        if (area.id == id) {
            return area;
        }
    }
    return std::nullopt;
}

int GameDatabase::geoAreaCount() const {
    return static_cast<int>(m_geoAreas.size());
}

// ============ Crafting Loading & Lookups ============

bool GameDatabase::loadCrafting(const std::filesystem::path& path) {
    auto craftingPath = path / "crafting.xml";
    if (!std::filesystem::exists(craftingPath)) {
        spdlog::warn("crafting.xml not found at {}", craftingPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(craftingPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open crafting.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("profession")) {
            CraftingProfession prof;
            QXmlStreamAttributes attrs = xml.attributes();
            
            prof.identifier = attrs.value("identifier").toString();
            prof.key = attrs.value("key").toString();
            prof.name = attrs.value("name").toString();
            
            // Parse profession tiers
            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("profession"))) {
                xml.readNext();
                
                if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("professionTier")) {
                    CraftingTier tier;
                    QXmlStreamAttributes tierAttrs = xml.attributes();
                    tier.identifier = tierAttrs.value("identifier").toInt();
                    tier.name = tierAttrs.value("name").toString();
                    
                    // Parse proficiency and mastery XP inside the tier
                    while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("professionTier"))) {
                        xml.readNext();
                        
                        if (xml.tokenType() == QXmlStreamReader::StartElement) {
                            if (xml.name() == QStringLiteral("proficiency")) {
                                tier.proficiencyXp = xml.attributes().value("xp").toInt();
                            } else if (xml.name() == QStringLiteral("mastery")) {
                                tier.masteryXp = xml.attributes().value("xp").toInt();
                            }
                        }
                    }
                    
                    prof.tiers.push_back(tier);
                }
            }
            
            if (!prof.key.isEmpty() && !prof.name.isEmpty()) {
                m_professions.push_back(prof);
                count++;
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} crafting professions from XML", count);
    return true;
}

std::vector<CraftingProfession> GameDatabase::getAllProfessions() const {
    return m_professions;
}

std::optional<CraftingProfession> GameDatabase::getProfession(const QString& key) const {
    for (const auto& prof : m_professions) {
        if (prof.key.compare(key, Qt::CaseInsensitive) == 0) {
            return prof;
        }
    }
    return std::nullopt;
}

int GameDatabase::professionCount() const {
    return static_cast<int>(m_professions.size());
}

// ============ Virtue Loading & Lookups ============

bool GameDatabase::loadVirtues(const std::filesystem::path& path) {
    auto virtuesPath = path / "virtues.xml";
    if (!std::filesystem::exists(virtuesPath)) {
        spdlog::warn("virtues.xml not found at {}", virtuesPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(virtuesPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open virtues.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("virtue")) {
            VirtueDef v;
            QXmlStreamAttributes attrs = xml.attributes();
            
            v.id = attrs.value("identifier").toString();
            v.key = attrs.value("key").toString();
            v.name = attrs.value("name").toString();
            v.maxRank = attrs.value("maxRank").toInt();
            
            if (!v.id.isEmpty() && !v.name.isEmpty()) {
                m_virtues.push_back(v);
                count++;
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} virtues from XML", count);
    return true;
}

std::vector<VirtueDef> GameDatabase::getAllVirtues() const {
    return m_virtues;
}

std::optional<VirtueDef> GameDatabase::getVirtue(const QString& id) const {
    for (const auto& v : m_virtues) {
        if (v.id == id) {
            return v;
        }
    }
    return std::nullopt;
}

int GameDatabase::virtueCount() const {
    return static_cast<int>(m_virtues.size());
}

// ============ Class Loading & Lookups ============

bool GameDatabase::loadClasses(const std::filesystem::path& path) {
    auto classesPath = path / "classes.xml";
    if (!std::filesystem::exists(classesPath)) {
        spdlog::warn("classes.xml not found at {}", classesPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(classesPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open classes.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("class")) {
            GameClass c;
            QXmlStreamAttributes attrs = xml.attributes();
            
            c.id = attrs.value("id").toString();
            c.code = attrs.value("code").toInt();
            c.key = attrs.value("key").toString();
            c.name = attrs.value("name").toString();
            c.abbreviation = attrs.value("abbreviation").toString();
            c.iconId = attrs.value("iconId").toInt();
            
            if (!c.id.isEmpty() && !c.name.isEmpty()) {
                m_classes.push_back(c);
                count++;
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} character classes from XML", count);
    return true;
}

std::vector<GameClass> GameDatabase::getAllClasses() const {
    return m_classes;
}

std::optional<GameClass> GameDatabase::getGameClass(const QString& key) const {
    for (const auto& c : m_classes) {
        if (c.key.compare(key, Qt::CaseInsensitive) == 0 ||
            c.name.compare(key, Qt::CaseInsensitive) == 0) {
            return c;
        }
    }
    return std::nullopt;
}

int GameDatabase::classCount() const {
    return static_cast<int>(m_classes.size());
}

std::optional<GameClass> GameDatabase::getClassByCode(int code) const {
    for (const auto& c : m_classes) {
        if (c.code == code) {
            return c;
        }
    }
    return std::nullopt;
}

// ============ Race Loading & Lookups ============

bool GameDatabase::loadRaces(const std::filesystem::path& path) {
    auto racesPath = path / "races.xml";
    if (!std::filesystem::exists(racesPath)) {
        spdlog::warn("races.xml not found at {}", racesPath.string());
        return false;
    }
    
    QFile file(QString::fromStdString(racesPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::error("Failed to open races.xml");
        return false;
    }
    
    int count = 0;
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("race")) {
            Race r;
            QXmlStreamAttributes attrs = xml.attributes();
            
            r.id = attrs.value("id").toString();
            r.code = attrs.value("code").toInt();
            r.key = attrs.value("key").toString();
            r.name = attrs.value("name").toString();
            r.iconId = attrs.value("iconId").toInt();
            
            if (!r.id.isEmpty() && !r.name.isEmpty()) {
                m_races.push_back(r);
                count++;
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} races from XML", count);
    return true;
}

std::vector<Race> GameDatabase::getAllRaces() const {
    return m_races;
}

std::optional<Race> GameDatabase::getRace(const QString& key) const {
    for (const auto& r : m_races) {
        if (r.key.compare(key, Qt::CaseInsensitive) == 0 ||
            r.name.compare(key, Qt::CaseInsensitive) == 0) {
            return r;
        }
    }
    return std::nullopt;
}

int GameDatabase::raceCount() const {
    return static_cast<int>(m_races.size());
}

std::optional<Race> GameDatabase::getRaceByCode(int code) const {
    for (const auto& r : m_races) {
        if (r.code == code) {
            return r;
        }
    }
    return std::nullopt;
}

} // namespace lotro
