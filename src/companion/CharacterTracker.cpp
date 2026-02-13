/**
 * LOTRO Launcher - Character Tracker Implementation
 * 
 * Full companion data persistence with JSON serialization.
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
        case CharacterClass::Mariner: return "Mariner";
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
    if (lower == "mariner") return CharacterClass::Mariner;
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

// ============ JSON Serialization Helpers ============

static json characterToJson(const Character& c) {
    json j;
    j["name"] = c.name.toStdString();
    j["server"] = c.server.toStdString();
    j["class"] = c.classString().toStdString();
    j["race"] = c.raceString().toStdString();
    j["level"] = c.level;
    j["account"] = c.accountName.toStdString();
    j["currentArea"] = c.currentArea.toStdString();
    j["virtueXP"] = c.virtueXP;
    j["destinyPoints"] = c.destinyPoints;
    j["morale"] = c.morale;
    j["maxMorale"] = c.maxMorale;
    j["power"] = c.power;
    j["maxPower"] = c.maxPower;
    j["gold"] = c.gold;
    j["silver"] = c.silver;
    j["copper"] = c.copper;

    j["lastPlayed"] = std::chrono::duration_cast<std::chrono::seconds>(
        c.lastPlayed.time_since_epoch()).count();
    j["created"] = std::chrono::duration_cast<std::chrono::seconds>(
        c.created.time_since_epoch()).count();
    j["hasExtendedData"] = c.hasExtendedData;
    
    // Extended data
    if (c.hasExtendedData) {
        // Virtues
        json virtuesArr = json::array();
        for (const auto& v : c.virtues) {
            virtuesArr.push_back({
                {"key", v.key.toStdString()},
                {"name", v.name.toStdString()},
                {"rank", v.rank},
                {"xp", v.xp}
            });
        }
        j["virtues"] = virtuesArr;
        
        // Factions
        json factionsArr = json::array();
        for (const auto& f : c.factions) {
            factionsArr.push_back({
                {"factionId", f.factionId},
                {"key", f.key.toStdString()},
                {"name", f.name.toStdString()},
                {"category", f.category.toStdString()},
                {"tier", f.tier},
                {"reputation", f.reputation}
            });
        }
        j["factions"] = factionsArr;
        
        // Crafting
        json craftingJson;
        craftingJson["vocation"] = c.crafting.vocation.toStdString();
        json profsArr = json::array();
        for (const auto& p : c.crafting.professions) {
            profsArr.push_back({
                {"name", p.name.toStdString()},
                {"tier", p.tier},
                {"proficiency", p.proficiency},
                {"mastery", p.mastery},
                {"hasMastered", p.hasMastered}
            });
        }
        craftingJson["professions"] = profsArr;
        j["crafting"] = craftingJson;
        
        // Equipped gear
        json gearJson = json::object();
        for (const auto& [slot, itemId] : c.equippedGear) {
            gearJson[slot.toStdString()] = itemId;
        }
        j["equippedGear"] = gearJson;
        
        // Titles, emotes, skills
        j["titles"] = c.titles;
        j["emotes"] = c.emotes;
        j["skills"] = c.skills;
        
        // Trait points
        json traitJson = json::object();
        for (const auto& [specId, points] : c.traitPoints) {
            traitJson[std::to_string(specId)] = points;
        }
        j["traitPoints"] = traitJson;
    }
    
    return j;
}

static Character characterFromJson(const json& j) {
    Character c;
    c.name = QString::fromStdString(j.value("name", ""));
    c.server = QString::fromStdString(j.value("server", ""));
    c.characterClass = parseCharacterClass(QString::fromStdString(j.value("class", "")));
    c.race = parseCharacterRace(QString::fromStdString(j.value("race", "")));
    c.level = j.value("level", 1);
    c.accountName = QString::fromStdString(j.value("account", ""));
    c.currentArea = QString::fromStdString(j.value("currentArea", ""));
    c.virtueXP = j.value("virtueXP", 0);
    c.destinyPoints = j.value("destinyPoints", 0);
    c.morale = j.value("morale", 0);
    c.maxMorale = j.value("maxMorale", 0);
    c.power = j.value("power", 0);
    c.maxPower = j.value("maxPower", 0);
    c.gold = j.value("gold", 0);
    c.silver = j.value("silver", 0);
    c.copper = j.value("copper", 0);

    
    int64_t lastPlayed = j.value("lastPlayed", 0);
    c.lastPlayed = std::chrono::system_clock::time_point(std::chrono::seconds(lastPlayed));
    
    int64_t created = j.value("created", 0);
    c.created = std::chrono::system_clock::time_point(std::chrono::seconds(created));
    
    c.hasExtendedData = j.value("hasExtendedData", false);
    
    // Extended data
    if (c.hasExtendedData) {
        // Virtues
        if (j.contains("virtues")) {
            for (const auto& vj : j["virtues"]) {
                SavedVirtue v;
                v.key = QString::fromStdString(vj.value("key", ""));
                v.name = QString::fromStdString(vj.value("name", ""));
                v.rank = vj.value("rank", 0);
                v.xp = vj.value("xp", 0);
                c.virtues.push_back(v);
            }
        }
        
        // Factions
        if (j.contains("factions")) {
            for (const auto& fj : j["factions"]) {
                SavedFaction f;
                f.factionId = fj.value("factionId", 0);
                f.key = QString::fromStdString(fj.value("key", ""));
                f.name = QString::fromStdString(fj.value("name", ""));
                f.category = QString::fromStdString(fj.value("category", ""));
                f.tier = fj.value("tier", 0);
                f.reputation = fj.value("reputation", 0);
                c.factions.push_back(f);
            }
        }
        
        // Crafting
        if (j.contains("crafting")) {
            const auto& cj = j["crafting"];
            c.crafting.vocation = QString::fromStdString(cj.value("vocation", ""));
            if (cj.contains("professions")) {
                for (const auto& pj : cj["professions"]) {
                    SavedCraftingProfession p;
                    p.name = QString::fromStdString(pj.value("name", ""));
                    p.tier = pj.value("tier", 0);
                    p.proficiency = pj.value("proficiency", 0);
                    p.mastery = pj.value("mastery", 0);
                    p.hasMastered = pj.value("hasMastered", false);
                    c.crafting.professions.push_back(p);
                }
            }
        }
        
        // Equipped gear
        if (j.contains("equippedGear")) {
            for (auto& [key, val] : j["equippedGear"].items()) {
                c.equippedGear[QString::fromStdString(key)] = val.get<int>();
            }
        }
        
        // Titles, emotes, skills
        if (j.contains("titles")) {
            c.titles = j["titles"].get<std::vector<int>>();
        }
        if (j.contains("emotes")) {
            c.emotes = j["emotes"].get<std::vector<int>>();
        }
        if (j.contains("skills")) {
            c.skills = j["skills"].get<std::vector<int>>();
        }
        
        // Trait points
        if (j.contains("traitPoints")) {
            for (auto& [key, val] : j["traitPoints"].items()) {
                c.traitPoints[std::stoi(key)] = val.get<int>();
            }
        }
    }
    
    return c;
}

// ============ Implementation Class ============

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
    return 0;
}

QString CharacterTracker::exportToJson() const {
    json j = json::array();
    for (const auto& c : m_impl->characters) {
        j.push_back(characterToJson(c));
    }
    return QString::fromStdString(j.dump(2));
}

int CharacterTracker::importFromJson(const QString& jsonStr) {
    try {
        json j = json::parse(jsonStr.toStdString());
        int count = 0;
        
        for (const auto& charJson : j) {
            Character c = characterFromJson(charJson);
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
        return true;
    }
    
    try {
        std::ifstream file(path);
        json j = json::parse(file);
        
        m_impl->characters.clear();
        
        for (const auto& charJson : j) {
            m_impl->characters.push_back(characterFromJson(charJson));
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
            j.push_back(characterToJson(c));
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
