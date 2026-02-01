#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <vector>
#include <map>

namespace lotro {

namespace dat { class DataFacade; }
class ProcessMemory;
struct CharacterInfo;

enum class ExtractableElement {
    BasicInfo,
    Crafting,
    Currency,
    CurrentGear,
    ArchiveGear,
    Bags,
    Reputation,
    Virtues,
    Deeds,
    Quests,
    Titles,
    SkirmishStats,
    Housing,
    FriendList,
    Kinship,
    Wardrobe,
    Outfits,
    Mounts,
    Emotes,
    Traits,
    Skills
};

struct ElementDefinition {
    ExtractableElement id;
    QString name;
    bool enabledByDefault;
};

class DataExporter : public QObject {
    Q_OBJECT
public:
    explicit DataExporter(dat::DataFacade* facade, ProcessMemory* memory, QObject* parent = nullptr);
    
    // Get all supported elements
    static std::vector<ElementDefinition> getSupportedElements();
    
    // Set output directory for exports
    void setOutputPath(const QString& path);
    
    // Start extraction
    void extract(const std::vector<ExtractableElement>& elements);

signals:
    void logMessage(const QString& msg);
    void extractionFinished();

private:
    dat::DataFacade* m_facade;
    ProcessMemory* m_memory;
    QString m_outputPath;
    
    void extractElement(ExtractableElement element, const CharacterInfo& info, QJsonObject& exportData);
    void saveExportToFile(const QJsonObject& data, const CharacterInfo& info);
};

} // namespace lotro
