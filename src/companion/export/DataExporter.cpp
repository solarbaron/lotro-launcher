#include "DataExporter.hpp"
#include <QThread>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>

#include "dat/DataFacade.hpp"
#include "companion/CharacterExtractor.hpp"
#include "companion/ProcessMemory.hpp"
#include "companion/GameDatabase.hpp"

#include <spdlog/spdlog.h>

namespace lotro {

DataExporter::DataExporter(dat::DataFacade* facade, ProcessMemory* memory, QObject* parent)
    : QObject(parent), m_facade(facade), m_memory(memory)
{
    // Default output path: ~/Documents/lotro-launcher/exports/
    m_outputPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) 
                   + "/lotro-launcher/exports";
}

std::vector<ElementDefinition> DataExporter::getSupportedElements() {
    return {
        {ExtractableElement::BasicInfo, "Basic Info (Character/Account)", true},
        {ExtractableElement::Currency, "Wallet/Currencies", true},
        {ExtractableElement::CurrentGear, "Current Gear", false},
        {ExtractableElement::ArchiveGear, "Backup current gear", false},
        {ExtractableElement::Reputation, "Reputation Status", false},
        {ExtractableElement::Virtues, "Virtues Status", false},
        {ExtractableElement::Crafting, "Crafting Status", false},
        {ExtractableElement::Deeds, "Deeds Status", false},
        {ExtractableElement::Quests, "Quests Status", false},
        {ExtractableElement::Titles, "Titles", false},
        {ExtractableElement::SkirmishStats, "Skirmish Stats", false},
        {ExtractableElement::Bags, "Bags", false},
        {ExtractableElement::Housing, "Housing", false},
        {ExtractableElement::FriendList, "Friends", false},
        {ExtractableElement::Kinship, "Kinship", false},
        {ExtractableElement::Wardrobe, "Wardrobe", false},
        {ExtractableElement::Outfits, "Outfits", false},
        {ExtractableElement::Traits, "Traits", false},
        {ExtractableElement::Skills, "Skills", false},
        {ExtractableElement::Emotes, "Emotes", false}
    };
}

void DataExporter::setOutputPath(const QString& path) {
    m_outputPath = path;
}

void DataExporter::extract(const std::vector<ExtractableElement>& elements) {
    emit logMessage("Starting extraction process...");
    emit logMessage(QString("Output directory: %1").arg(m_outputPath));
    
    // Create CharacterExtractor once and reuse
    CharacterExtractor extractor(m_facade->gamePath());
    if (!extractor.connect()) {
        emit logMessage("[ERROR] Failed to connect to LOTRO client!");
        emit logMessage("Make sure the game is running and you are logged in.");
        emit extractionFinished();
        return;
    }
    
    // Extract character info (needed for most operations)
    auto charInfo = extractor.extractCharacter();
    if (!charInfo) {
        emit logMessage("[ERROR] Failed to extract character data!");
        emit logMessage("Make sure you are logged in with a character.");
        emit extractionFinished();
        return;
    }
    
    // Extract full data for extended elements
    auto fullData = extractor.extractFullData();
    
    emit logMessage(QString("Connected to character: %1 (Level %2 %3)")
                    .arg(charInfo->name)
                    .arg(charInfo->level)
                    .arg(charInfo->className));
    emit logMessage("-----------------------------------");
    
    // Create JSON export structure
    QJsonObject exportData;
    exportData["exportTimestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    exportData["exporterVersion"] = "1.0.0";
    exportData["characterName"] = charInfo->name;
    exportData["server"] = charInfo->server;
    
    // Process each element
    for (const auto& el : elements) {
        extractElement(el, *charInfo, fullData, exportData);
        QThread::msleep(50); // Allow UI updates
    }
    
    // Save to file
    emit logMessage("-----------------------------------");
    saveExportToFile(exportData, *charInfo);
    
    emit logMessage("===================================");
    emit logMessage("Extraction complete!");
    emit extractionFinished();
}

void DataExporter::extractElement(ExtractableElement element, const CharacterInfo& info, const std::optional<CharacterData>& fullData, QJsonObject& exportData) {
    QString name = "Unknown";
    for(const auto& def : getSupportedElements()) {
        if(def.id == element) {
            name = def.name;
            break;
        }
    }
    
    emit logMessage(QString("Extracting %1...").arg(name));
    
    switch(element) {
        case ExtractableElement::BasicInfo:
            {
                QJsonObject basicInfo;
                basicInfo["name"] = info.name;
                basicInfo["surname"] = info.surname;
                basicInfo["level"] = info.level;
                basicInfo["class"] = info.className;
                basicInfo["race"] = info.race;
                basicInfo["server"] = info.server;
                basicInfo["account"] = info.account;
                basicInfo["accountType"] = static_cast<int>(info.accountType);
                
                // Vitals
                QJsonObject vitals;
                vitals["morale"] = info.morale;
                vitals["maxMorale"] = info.maxMorale;
                vitals["power"] = info.power;
                vitals["maxPower"] = info.maxPower;
                basicInfo["vitals"] = vitals;
                
                exportData["basicInfo"] = basicInfo;
                
                emit logMessage(QString("  Name: %1 %2").arg(info.name).arg(info.surname));
                emit logMessage(QString("  Level: %1 %2 %3").arg(info.level).arg(info.race).arg(info.className));
                emit logMessage(QString("  Server: %1").arg(info.server));
                emit logMessage(QString("  Account: %1 (Type: %2)").arg(info.account).arg(static_cast<int>(info.accountType)));
                emit logMessage(QString("  Morale: %1/%2").arg(info.morale).arg(info.maxMorale));
                emit logMessage(QString("  Power: %1/%2").arg(info.power).arg(info.maxPower));
                emit logMessage("  [OK] Basic Info extracted.");
            }
            break;
            
        case ExtractableElement::Currency:
            {
                QJsonObject currency;
                currency["gold"] = info.gold;
                currency["silver"] = info.silver;
                currency["copper"] = info.copper;
                currency["totalCopper"] = info.totalCopper();
                currency["destinyPoints"] = info.destinyPoints;
                
                // Extended wallet currencies
                if (fullData && !fullData->wallet.empty()) {
                    QJsonObject wallet;
                    static const QMap<int, QString> currencyNames = {
                        {1, "marks"}, {2, "medallions"}, {3, "seals"},
                        {4, "mithrilCoins"}, {5, "commendations"}, {6, "skirmishMarks"},
                        {7, "fateTokens"}, {8, "hobbitPresents"}, {9, "silverTokens"},
                        {10, "giftMathom"}, {11, "craftingGuild"}, {12, "anniversaryTokens"},
                        {13, "festivalTokens"}, {14, "figments"}, {15, "moriaShards"}
                    };
                    for (auto it = fullData->wallet.begin(); it != fullData->wallet.end(); ++it) {
                        QString name = currencyNames.value(it->first, QString::number(it->first));
                        wallet[name] = it->second;
                    }
                    currency["wallet"] = wallet;
                    emit logMessage(QString("  Wallet currencies: %1").arg(fullData->wallet.size()));
                }
                
                exportData["currency"] = currency;
                
                emit logMessage(QString("  Money: %1").arg(info.formattedMoney()));
                emit logMessage(QString("  Total Copper: %1").arg(info.totalCopper()));
                emit logMessage(QString("  Destiny Points: %1").arg(info.destinyPoints));
                emit logMessage("  [OK] Currency extracted.");
            }
            break;
            
        case ExtractableElement::Crafting:
            {
                if (fullData && !fullData->crafting.professions.empty()) {
                    QJsonObject crafting;
                    QJsonArray professions;
                    for (const auto& prof : fullData->crafting.professions) {
                        QJsonObject p;
                        p["name"] = prof.name;
                        p["tier"] = prof.tier;
                        p["proficiency"] = prof.proficiency;
                        p["mastery"] = prof.mastery;
                        p["hasMastered"] = prof.hasMastered;
                        professions.append(p);
                    }
                    crafting["professions"] = professions;
                    exportData["crafting"] = crafting;
                    emit logMessage(QString("  Extracted %1 crafting professions.").arg(fullData->crafting.professions.size()));
                } else {
                    emit logMessage("  [SKIP] No crafting professions found.");
                }
            }
            break;
            
        case ExtractableElement::Reputation:
            {
                if (fullData && !fullData->factions.empty()) {
                    QJsonObject reputation;
                    QJsonArray factions;
                    for (const auto& f : fullData->factions) {
                        QJsonObject fj;
                        fj["key"] = f.key;
                        fj["name"] = f.name;
                        fj["tier"] = f.tier;
                        fj["reputation"] = f.reputation;
                        factions.append(fj);
                    }
                    reputation["factions"] = factions;
                    exportData["reputation"] = reputation;
                    emit logMessage(QString("  Extracted %1 faction reputations.").arg(fullData->factions.size()));
                } else {
                    emit logMessage("  [SKIP] No faction reputations found.");
                }
            }
            break;
            
        case ExtractableElement::Virtues:
            {
                if (fullData && !fullData->virtues.empty()) {
                    QJsonObject virtues;
                    QJsonArray virtueArray;
                    for (const auto& v : fullData->virtues) {
                        if (v.rank > 0 || v.xp > 0) {
                            QJsonObject vj;
                            vj["key"] = v.key;
                            vj["name"] = v.name;
                            vj["rank"] = v.rank;
                            vj["xp"] = v.xp;
                            virtueArray.append(vj);
                        }
                    }
                    virtues["virtues"] = virtueArray;
                    exportData["virtues"] = virtues;
                    emit logMessage(QString("  Extracted %1 virtue statuses.").arg(virtueArray.size()));
                } else {
                    emit logMessage("  [SKIP] No virtue data found.");
                }
            }
            break;
            
        case ExtractableElement::CurrentGear:
            {
                if (fullData && !fullData->equippedGear.empty()) {
                    QJsonObject gear;
                    for (auto it = fullData->equippedGear.begin(); it != fullData->equippedGear.end(); ++it) {
                        gear[it->first] = it->second;
                    }
                    exportData["gear"] = gear;
                    emit logMessage(QString("  Extracted %1 equipped items.").arg(fullData->equippedGear.size()));
                } else {
                    emit logMessage("  [SKIP] No equipped gear found.");
                }
            }
            break;
            
        case ExtractableElement::Bags:
            {
                QJsonObject items;
                items["status"] = "not_implemented";
                items["note"] = "Bag extraction requires container traversal";
                exportData["bags"] = items;
                emit logMessage("  [PENDING] Bag extraction requires container traversal.");
            }
            break;
            
        case ExtractableElement::Titles:
            {
                // Export all known titles from GameDatabase
                auto& db = GameDatabase::instance();
                QJsonObject titlesObj;
                QJsonArray titlesArray;
                int count = 0;
                for (const auto& t : db.searchTitles("")) {
                    QJsonObject tj;
                    tj["id"] = t.id;
                    tj["name"] = t.name;
                    tj["category"] = t.description;
                    titlesArray.append(tj);
                    count++;
                    if (count >= 100) break; // Limit for export
                }
                titlesObj["available"] = db.titleCount();
                titlesObj["sample"] = titlesArray;
                exportData["titles"] = titlesObj;
                emit logMessage(QString("  Database contains %1 known titles.").arg(db.titleCount()));
            }
            break;
            
        case ExtractableElement::Emotes:
            {
                // Export all known emotes from GameDatabase
                auto& db = GameDatabase::instance();
                QJsonObject emotesObj;
                QJsonArray emotesArray;
                for (const auto& e : db.getAllEmotes()) {
                    QJsonObject ej;
                    ej["id"] = e.id;
                    ej["command"] = e.command;
                    ej["type"] = e.source; // Default or Special
                    emotesArray.append(ej);
                }
                emotesObj["emotes"] = emotesArray;
                exportData["emotes"] = emotesObj;
                emit logMessage(QString("  Database contains %1 known emotes.").arg(db.emoteCount()));
            }
            break;
            
        case ExtractableElement::Skills:
            {
                auto& db = GameDatabase::instance();
                QJsonObject skills;
                skills["totalKnown"] = db.skillCount();
                skills["status"] = "database_only";
                skills["note"] = "Skills data loaded - character-specific skills require memory array extraction";
                exportData["skills"] = skills;
                emit logMessage(QString("  Database contains %1 known skills.").arg(db.skillCount()));
            }
            break;
            
        case ExtractableElement::Traits:
            {
                auto& db = GameDatabase::instance();
                QJsonObject traits;
                traits["totalKnown"] = db.traitCount();
                traits["status"] = "database_only";
                traits["note"] = "Traits data loaded - character-specific traits require memory array extraction";
                exportData["traits"] = traits;
                emit logMessage(QString("  Database contains %1 known traits.").arg(db.traitCount()));
            }
            break;
            
        case ExtractableElement::Deeds:
            {
                auto& db = GameDatabase::instance();
                QJsonObject deedsObj;
                deedsObj["totalKnown"] = db.deedCount();
                deedsObj["status"] = "database_only";
                deedsObj["note"] = "Deed completion status requires memory extraction";
                exportData["deeds"] = deedsObj;
                emit logMessage(QString("  Database contains %1 known deeds.").arg(db.deedCount()));
            }
            break;
            
        case ExtractableElement::Quests:
            {
                auto& db = GameDatabase::instance();
                QJsonObject quests;
                quests["totalKnown"] = db.questCount();
                quests["status"] = "database_only";
                quests["note"] = "Quest completion requires memory extraction";
                exportData["quests"] = quests;
                emit logMessage(QString("  Database contains %1 known quests.").arg(db.questCount()));
            }
            break;
            
        case ExtractableElement::SkirmishStats:
            {
                QJsonObject skirmish;
                skirmish["status"] = "not_implemented";
                skirmish["note"] = "Skirmish stats require memory extraction";
                exportData["skirmishStats"] = skirmish;
                emit logMessage("  Skirmish stats pending implementation.");
            }
            break;
            
        case ExtractableElement::Housing:
            {
                QJsonObject housing;
                housing["status"] = "not_implemented";
                housing["note"] = "Housing data requires memory extraction";
                exportData["housing"] = housing;
                emit logMessage("  Housing data pending implementation.");
            }
            break;
            
        case ExtractableElement::FriendList:
            {
                QJsonObject friends;
                friends["status"] = "not_implemented";
                friends["note"] = "Friend list requires memory extraction";
                exportData["friends"] = friends;
                emit logMessage("  Friend list pending implementation.");
            }
            break;
            
        case ExtractableElement::Kinship:
            {
                QJsonObject kinship;
                kinship["status"] = "not_implemented";
                kinship["note"] = "Kinship data requires memory extraction";
                exportData["kinship"] = kinship;
                emit logMessage("  Kinship data pending implementation.");
            }
            break;
            
        case ExtractableElement::Wardrobe:
            {
                auto& db = GameDatabase::instance();
                QJsonObject wardrobe;
                wardrobe["totalCosmetics"] = db.cosmeticCount();
                wardrobe["status"] = "database_only";
                wardrobe["note"] = "Cosmetics database loaded - character wardrobe requires memory extraction";
                exportData["wardrobe"] = wardrobe;
                emit logMessage(QString("  Database contains %1 known cosmetic items.").arg(db.cosmeticCount()));
            }
            break;
            
        case ExtractableElement::Outfits:
            {
                QJsonObject outfits;
                outfits["status"] = "pending";
                outfits["note"] = "Outfits require equipped cosmetic slot reading";
                exportData["outfits"] = outfits;
                emit logMessage("  Outfits pending memory extraction.");
            }
            break;
            
        case ExtractableElement::Mounts:
            {
                auto& db = GameDatabase::instance();
                QJsonObject mounts;
                mounts["totalCollections"] = db.collectionCount();
                mounts["status"] = "database_only";
                mounts["note"] = "Collections database loaded - character mounts require memory extraction";
                exportData["mounts"] = mounts;
                emit logMessage(QString("  Database contains %1 collection items (mounts/pets).").arg(db.collectionCount()));
            }
            break;
            
        default:
            {
                emit logMessage("  [SKIP] Unknown element type.");
            }
            break;
    }
}

void DataExporter::saveExportToFile(const QJsonObject& data, const CharacterInfo& info) {
    // Create output directory if needed
    QDir dir(m_outputPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            emit logMessage(QString("[ERROR] Failed to create output directory: %1").arg(m_outputPath));
            return;
        }
    }
    
    // Generate filename: CharacterName_Server_YYYYMMDD_HHMMSS.json
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString charName = info.name.isEmpty() ? "Unknown" : info.name;
    QString server = info.server.isEmpty() ? "Server" : info.server;
    
    // Sanitize names for filesystem
    charName.replace(QRegularExpression("[^a-zA-Z0-9]"), "_");
    server.replace(QRegularExpression("[^a-zA-Z0-9]"), "_");
    
    QString filename = QString("%1_%2_%3.json")
                       .arg(charName)
                       .arg(server)
                       .arg(timestamp);
    
    QString filepath = m_outputPath + "/" + filename;
    
    QFile file(filepath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument doc(data);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        
        emit logMessage(QString("Saved export to:"));
        emit logMessage(QString("  %1").arg(filepath));
        spdlog::info("Exported character data to: {}", filepath.toStdString());
    } else {
        emit logMessage(QString("[ERROR] Failed to save export to: %1").arg(filepath));
        spdlog::error("Failed to save export to: {}", filepath.toStdString());
    }
}

} // namespace lotro
