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
        extractElement(el, *charInfo, exportData);
        QThread::msleep(50); // Allow UI updates
    }
    
    // Save to file
    emit logMessage("-----------------------------------");
    saveExportToFile(exportData, *charInfo);
    
    emit logMessage("===================================");
    emit logMessage("Extraction complete!");
    emit extractionFinished();
}

void DataExporter::extractElement(ExtractableElement element, const CharacterInfo& info, QJsonObject& exportData) {
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
                currency["lotroPoints"] = info.lotroPoints;
                
                exportData["currency"] = currency;
                
                emit logMessage(QString("  Money: %1").arg(info.formattedMoney()));
                emit logMessage(QString("  Total Copper: %1").arg(info.totalCopper()));
                emit logMessage(QString("  Destiny Points: %1").arg(info.destinyPoints));
                emit logMessage(QString("  LOTRO Points: %1").arg(info.lotroPoints));
                emit logMessage("  [OK] Currency extracted.");
            }
            break;
            
        case ExtractableElement::Crafting:
            {
                QJsonObject crafting;
                crafting["status"] = "not_implemented";
                crafting["note"] = "Crafting extraction requires profession property IDs";
                exportData["crafting"] = crafting;
                emit logMessage("  [PENDING] Crafting extraction requires additional property research.");
            }
            break;
            
        case ExtractableElement::Reputation:
            {
                QJsonObject reputation;
                reputation["status"] = "not_implemented";
                reputation["note"] = "Reputation extraction requires faction property IDs";
                exportData["reputation"] = reputation;
                emit logMessage("  [PENDING] Reputation extraction requires additional property research.");
            }
            break;
            
        case ExtractableElement::Virtues:
            {
                QJsonObject virtues;
                virtues["status"] = "not_implemented";
                virtues["note"] = "Virtues extraction requires virtue property IDs";
                exportData["virtues"] = virtues;
                emit logMessage("  [PENDING] Virtues extraction requires additional property research.");
            }
            break;
            
        case ExtractableElement::CurrentGear:
        case ExtractableElement::Bags:
            {
                QJsonObject items;
                items["status"] = "not_implemented";
                items["note"] = "Item extraction requires entity scanning (complex)";
                QString key = (element == ExtractableElement::CurrentGear) ? "gear" : "bags";
                exportData[key] = items;
                emit logMessage("  [PENDING] Item extraction requires entity scanning.");
            }
            break;
            
        default:
            {
                emit logMessage("  [SKIP] Not implemented yet.");
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
