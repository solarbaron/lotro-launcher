/**
 * LOTRO Launcher - Character Tracker Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CharacterTracker.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace lotro {

using json = nlohmann::json;

// Helper functions for class/race string conversion
QString Character::classString() const {
    switch (characterClass) {
        case CharacterClass::Beorning: return "Beorning";
        case CharacterClass::Brawler: return "Brawler";
        case CharacterClass::Burglar: return "Burglar";
        case CharacterClass::Captain: return "Captain";
        case CharacterClass::Champion: return "Champion";
        case CharacterClass::Guardian: return "Guardian";
        case CharacterClass::Hunter: return "Hunter";
        case CharacterClass::LoreMaster: return "Lore-master";
        case CharacterClass::Minstrel: return "Minstrel";
        case CharacterClass::RuneKeeper: return "Rune-keeper";
        case CharacterClass::Warden: return "Warden";
        default: return "Unknown";
    }
}

QString Character::raceString() const {
    switch (race) {
        case CharacterRace::Dwarf: return "Dwarf";
        case CharacterRace::Elf: return "Elf";
        case CharacterRace::HighElf: return "High Elf";
        case CharacterRace::Hobbit: return "Hobbit";
        case CharacterRace::Man: return "Man";
        case CharacterRace::Beorning: return "Beorning";
        case CharacterRace::StoutAxe: return "Stout-axe";
        case CharacterRace::RiverHobbit: return "River Hobbit";
        default: return "Unknown";
    }
}

CharacterClass parseCharacterClass(const QString& str) {
    QString lower = str.toLower();
    if (lower == "beorning") return CharacterClass::Beorning;
    if (lower == "brawler") return CharacterClass::Brawler;
    if (lower == "burglar") return CharacterClass::Burglar;
    if (lower == "captain") return CharacterClass::Captain;
    if (lower == "champion") return CharacterClass::Champion;
    if (lower == "guardian") return CharacterClass::Guardian;
    if (lower == "hunter") return CharacterClass::Hunter;
    if (lower == "lore-master" || lower == "loremaster") return CharacterClass::LoreMaster;
    if (lower == "minstrel") return CharacterClass::Minstrel;
    if (lower == "rune-keeper" || lower == "runekeeper") return CharacterClass::RuneKeeper;
    if (lower == "warden") return CharacterClass::Warden;
    return CharacterClass::Unknown;
}

CharacterRace parseCharacterRace(const QString& str) {
    QString lower = str.toLower();
    if (lower == "dwarf") return CharacterRace::Dwarf;
    if (lower == "elf") return CharacterRace::Elf;
    if (lower == "high elf" || lower == "highelf") return CharacterRace::HighElf;
    if (lower == "hobbit") return CharacterRace::Hobbit;
    if (lower == "man") return CharacterRace::Man;
    if (lower == "beorning") return CharacterRace::Beorning;
    if (lower == "stout-axe" || lower == "stoutaxe") return CharacterRace::StoutAxe;
    if (lower == "river hobbit" || lower == "riverhobbit") return CharacterRace::RiverHobbit;
    return CharacterRace::Unknown;
}

// Implementation class
class CharacterTracker::Impl {
public:
    std::filesystem::path dataDir;
    std::vector<Character> characters;
    
    std::filesystem::path getFilePath() const {
        return dataDir / "characters.json";
    }
};

CharacterTracker::CharacterTracker(const std::filesystem::path& dataDir)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->dataDir = dataDir;
    
    // Create directory if it doesn't exist
    if (!std::filesystem::exists(dataDir)) {
        std::filesystem::create_directories(dataDir);
    }
    
    load();
}

CharacterTracker::~CharacterTracker() {
    save();
}

std::vector<Character> CharacterTracker::getCharacters() const {
    return m_impl->characters;
}

std::vector<Character> CharacterTracker::getCharacters(const QString& server) const {
    std::vector<Character> result;
    for (const auto& c : m_impl->characters) {
        if (c.server == server) {
            result.push_back(c);
        }
    }
    return result;
}

std::vector<Character> CharacterTracker::getCharactersByAccount(const QString& account) const {
    std::vector<Character> result;
    for (const auto& c : m_impl->characters) {
        if (c.accountName == account) {
            result.push_back(c);
        }
    }
    return result;
}

std::optional<Character> CharacterTracker::getCharacter(const QString& name, const QString& server) const {
    for (const auto& c : m_impl->characters) {
        if (c.name == name && c.server == server) {
            return c;
        }
    }
    return std::nullopt;
}

void CharacterTracker::saveCharacter(const Character& character) {
    // Check if character already exists
    for (auto& c : m_impl->characters) {
        if (c.name == character.name && c.server == character.server) {
            // Update existing
            c = character;
            save();
            spdlog::info("Updated character: {} on {}", character.name.toStdString(), character.server.toStdString());
            return;
        }
    }
    
    // Add new character
    m_impl->characters.push_back(character);
    save();
    spdlog::info("Added new character: {} on {}", character.name.toStdString(), character.server.toStdString());
}

bool CharacterTracker::removeCharacter(const QString& name, const QString& server) {
    auto it = std::remove_if(m_impl->characters.begin(), m_impl->characters.end(),
        [&](const Character& c) {
            return c.name == name && c.server == server;
        });
    
    if (it != m_impl->characters.end()) {
        m_impl->characters.erase(it, m_impl->characters.end());
        save();
        spdlog::info("Removed character: {} on {}", name.toStdString(), server.toStdString());
        return true;
    }
    return false;
}

void CharacterTracker::updateLastPlayed(const QString& name, const QString& server, 
                                         std::chrono::system_clock::time_point time) {
    for (auto& c : m_impl->characters) {
        if (c.name == name && c.server == server) {
            c.lastPlayed = time;
            save();
            return;
        }
    }
}

int CharacterTracker::importFromClient() {
    // Placeholder - would integrate with CharacterExtractor
    return 0;
}

QString CharacterTracker::exportToJson() const {
    json j = json::array();
    
    for (const auto& c : m_impl->characters) {
        json charJson;
        charJson["name"] = c.name.toStdString();
        charJson["server"] = c.server.toStdString();
        charJson["class"] = c.classString().toStdString();
        charJson["race"] = c.raceString().toStdString();
        charJson["level"] = c.level;
        charJson["account"] = c.accountName.toStdString();
        charJson["currentArea"] = c.currentArea.toStdString();
        charJson["virtueXP"] = c.virtueXP;
        charJson["destinyPoints"] = c.destinyPoints;
        charJson["lastPlayed"] = std::chrono::duration_cast<std::chrono::seconds>(
            c.lastPlayed.time_since_epoch()).count();
        charJson["created"] = std::chrono::duration_cast<std::chrono::seconds>(
            c.created.time_since_epoch()).count();
        j.push_back(charJson);
    }
    
    return QString::fromStdString(j.dump(2));
}

int CharacterTracker::importFromJson(const QString& jsonStr) {
    try {
        json j = json::parse(jsonStr.toStdString());
        int count = 0;
        
        for (const auto& charJson : j) {
            Character c;
            c.name = QString::fromStdString(charJson.value("name", ""));
            c.server = QString::fromStdString(charJson.value("server", ""));
            c.characterClass = parseCharacterClass(QString::fromStdString(charJson.value("class", "")));
            c.race = parseCharacterRace(QString::fromStdString(charJson.value("race", "")));
            c.level = charJson.value("level", 1);
            c.accountName = QString::fromStdString(charJson.value("account", ""));
            c.currentArea = QString::fromStdString(charJson.value("currentArea", ""));
            c.virtueXP = charJson.value("virtueXP", 0);
            c.destinyPoints = charJson.value("destinyPoints", 0);
            
            int64_t lastPlayed = charJson.value("lastPlayed", 0);
            c.lastPlayed = std::chrono::system_clock::time_point(std::chrono::seconds(lastPlayed));
            
            int64_t created = charJson.value("created", 0);
            c.created = std::chrono::system_clock::time_point(std::chrono::seconds(created));
            
            if (!c.name.isEmpty() && !c.server.isEmpty()) {
                saveCharacter(c);
                count++;
            }
        }
        
        return count;
    } catch (const std::exception& e) {
        spdlog::error("Failed to import characters from JSON: {}", e.what());
        return 0;
    }
}

bool CharacterTracker::load() {
    auto path = m_impl->getFilePath();
    if (!std::filesystem::exists(path)) {
        return true; // Empty is fine
    }
    
    try {
        std::ifstream file(path);
        json j = json::parse(file);
        
        m_impl->characters.clear();
        
        for (const auto& charJson : j) {
            Character c;
            c.name = QString::fromStdString(charJson.value("name", ""));
            c.server = QString::fromStdString(charJson.value("server", ""));
            c.characterClass = parseCharacterClass(QString::fromStdString(charJson.value("class", "")));
            c.race = parseCharacterRace(QString::fromStdString(charJson.value("race", "")));
            c.level = charJson.value("level", 1);
            c.accountName = QString::fromStdString(charJson.value("account", ""));
            c.currentArea = QString::fromStdString(charJson.value("currentArea", ""));
            c.virtueXP = charJson.value("virtueXP", 0);
            c.destinyPoints = charJson.value("destinyPoints", 0);
            
            int64_t lastPlayed = charJson.value("lastPlayed", 0);
            c.lastPlayed = std::chrono::system_clock::time_point(std::chrono::seconds(lastPlayed));
            
            int64_t created = charJson.value("created", 0);
            c.created = std::chrono::system_clock::time_point(std::chrono::seconds(created));
            
            m_impl->characters.push_back(c);
        }
        
        spdlog::info("Loaded {} characters from {}", m_impl->characters.size(), path.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load characters: {}", e.what());
        return false;
    }
}

bool CharacterTracker::save() {
    try {
        json j = json::array();
        
        for (const auto& c : m_impl->characters) {
            json charJson;
            charJson["name"] = c.name.toStdString();
            charJson["server"] = c.server.toStdString();
            charJson["class"] = c.classString().toStdString();
            charJson["race"] = c.raceString().toStdString();
            charJson["level"] = c.level;
            charJson["account"] = c.accountName.toStdString();
            charJson["currentArea"] = c.currentArea.toStdString();
            charJson["virtueXP"] = c.virtueXP;
            charJson["destinyPoints"] = c.destinyPoints;
            charJson["lastPlayed"] = std::chrono::duration_cast<std::chrono::seconds>(
                c.lastPlayed.time_since_epoch()).count();
            charJson["created"] = std::chrono::duration_cast<std::chrono::seconds>(
                c.created.time_since_epoch()).count();
            j.push_back(charJson);
        }
        
        auto path = m_impl->getFilePath();
        std::ofstream file(path);
        file << j.dump(2);
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save characters: {}", e.what());
        return false;
    }
}

} // namespace lotro
