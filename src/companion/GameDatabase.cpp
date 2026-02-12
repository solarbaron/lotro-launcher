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
        
        // Load titles, emotes, skills, traits, quests, collections, cosmetics
        loadTitles(loreDir);
        loadEmotes(loreDir);
        loadSkills(loreDir);
        loadTraits(loreDir);
        loadQuests(loreDir);
        loadCollections(loreDir);
        loadCosmetics(loreDir);
    } else {
        spdlog::info("No lore directory found, will use JSON fallback if available");
    }
    
    m_loaded = true;
    spdlog::info("Game database loaded: {} deeds, {} recipes, {} titles, {} emotes, {} skills, {} traits, {} quests, {} collections, {} cosmetics",
                 m_deeds.size(), m_recipes.size(), m_titles.size(), m_emotes.size(),
                 m_skills.size(), m_traits.size(), m_quests.size(), m_collections.size(), m_cosmetics.size());
    
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
            
            // Parse child elements for ingredients and result
            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("recipe"))) {
                xml.readNext();
                
                if (xml.tokenType() == QXmlStreamReader::StartElement) {
                    if (xml.name() == QStringLiteral("ingredient")) {
                        int quantity = xml.attributes().value("quantity").toInt();
                        if (quantity == 0) quantity = 1;
                        
                        // Get the ingredient item
                        while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("ingredient"))) {
                            xml.readNext();
                            if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("ingredientItem")) {
                                Recipe::Ingredient ing;
                                ing.itemId = xml.attributes().value("itemId").toString();
                                ing.name = xml.attributes().value("name").toString();
                                ing.quantity = quantity;
                                recipe.ingredients.push_back(ing);
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

} // namespace lotro
