/**
 * @file DataFacade.cpp
 * @brief Implementation of high-level DAT file access
 */

#include "DataFacade.hpp"
#include "DataFacade.hpp"
#include "PropertyDefinitionsLoader.hpp"
#include "BufferUtils.hpp"

#include <QDir>
#include <QFileInfo>
#include <spdlog/spdlog.h>

namespace lotro::dat {

DataFacade::DataFacade(const QString& gamePath)
    : m_gamePath(gamePath)
{
}

DataFacade::~DataFacade() {
    dispose();
}

bool DataFacade::initialize() {
    if (isInitialized()) {
        return true;
    }
    return openDatFiles();
}

bool DataFacade::openDatFiles() {
    QDir gameDir(m_gamePath);
    if (!gameDir.exists()) {
        spdlog::error("Game directory does not exist: {}", m_gamePath.toStdString());
        return false;
    }
    
    // Look for DAT files - they are typically named:
    // - client_local_English.dat (or other languages)
    // - client_general.dat
    // - client_gamelogic.dat (contains property definitions at 0x34000000)
    // - client_surface.dat
    // - client_highres.dat
    // etc.
    
    QStringList datFilters;
    // Property definitions are in gamelogic.dat, so we must include it
    datFilters << "client_gamelogic.dat" << "client_local_*.dat" << "client_general.dat";
    
    QFileInfoList datFiles = gameDir.entryInfoList(datFilters, QDir::Files);
    
    if (datFiles.isEmpty()) {
        spdlog::warn("No DAT files found in {}", m_gamePath.toStdString());
        
        // Also try looking directly for any .dat files as fallback
        datFilters.clear();
        datFilters << "*.dat";
        datFiles = gameDir.entryInfoList(datFilters, QDir::Files);
    }
    
    for (const QFileInfo& fi : datFiles) {
        spdlog::info("Opening DAT file: {}", fi.fileName().toStdString());
        
        auto archive = std::make_unique<DatArchive>(fi.absoluteFilePath());
        if (archive->open()) {
            m_archives.push_back(std::move(archive));
        } else {
            spdlog::warn("Failed to open DAT file: {}", fi.fileName().toStdString());
        }
    }
    
    if (m_archives.empty()) {
        spdlog::error("No DAT files could be opened");
        return false;
    }
    
    spdlog::info("Opened {} DAT archives", m_archives.size());
    return true;
}

PropertiesRegistry* DataFacade::getPropertiesRegistry() {
    if (m_propertiesRegistry) {
        return m_propertiesRegistry.get();
    }
    
    // Load the properties data
    QByteArray data = loadData(PROPERTIES_DATA_ID);
    if (data.isEmpty()) {
        spdlog::error("Failed to load properties data (ID 0x{:08X})", PROPERTIES_DATA_ID);
        return nullptr;
    }
    
    // Decode the properties
    PropertyDefinitionsLoader loader;
    m_propertiesRegistry = loader.decodeMasterProperty(data);
    
    if (!m_propertiesRegistry) {
        spdlog::error("Failed to decode properties registry");
        return nullptr;
    }
    
    // Log some known properties for debugging
    if (auto nameProp = m_propertiesRegistry->getPropertyDefByName("Name")) {
        spdlog::info("Found 'Name' property: ID={}", nameProp->propertyId());
    }
    if (auto levelProp = m_propertiesRegistry->getPropertyDefByName("Advancement_Level")) {
        spdlog::info("Found 'Advancement_Level' property: ID={}", levelProp->propertyId());
    }
    
    return m_propertiesRegistry.get();
}

QByteArray DataFacade::loadData(uint64_t dataId) {
    if (!isInitialized() && !initialize()) {
        return QByteArray();
    }
    
    // Try each archive until we find the data
    for (auto& archive : m_archives) {
        QByteArray data = archive->loadData(dataId);
        if (!data.isEmpty()) {
            return data;
        }
    }
    
    return QByteArray();
}

void DataFacade::dispose() {
    m_propertiesRegistry.reset();
    m_archives.clear();
}

QString DataFacade::resolveString(uint32_t tableId, uint32_t tokenId) {
    // spdlog::info("Resolving String: Table={}, Token={}", tableId, tokenId);
    
    QByteArray data = loadData(tableId);
    if (data.isEmpty()) {
        spdlog::warn("String Table {} not found", tableId);
        return QString();
    }
    
    // spdlog::info("String Table {} loaded, size={}", tableId, data.size());
    
    const char* ptr = data.constData();
    const char* end = ptr + data.size();
    
    // Header
    uint32_t did = BufferUtils::readUInt32(ptr);
    if (did != tableId) {
        spdlog::warn("Table ID mismatch: Expected {}, Got {}", tableId, did);
    }
    BufferUtils::readUInt32(ptr); // unknown
    int nbEntries = BufferUtils::readTSize(ptr);
    
    for (int i = 0; i < nbEntries && ptr < end; i++) {
        uint32_t token = BufferUtils::readUInt32(ptr);
        BufferUtils::readUInt32(ptr); // unknown
        uint32_t labelPartsCount = BufferUtils::readUInt32(ptr);
        
        QString resultStr;
        bool found = (token == tokenId);
        
        if (found) {
             spdlog::info("Found token {} at index {} (Table {})", token, i, tableId);
        }
        
        for (uint32_t j = 0; j < labelPartsCount; j++) {
            QString part = BufferUtils::readPrefixedUtf16String(ptr);
            if (found) {
                resultStr.append(part);
            }
        }
        
        uint32_t nbVariables = BufferUtils::readUInt32(ptr);
        for (uint32_t j = 0; j < nbVariables; j++) {
            BufferUtils::readUInt32(ptr); // variableID
        }
        
        bool hasVarNames = BufferUtils::readBoolean(ptr);
        if (hasVarNames) {
            uint32_t varNamesCount = BufferUtils::readUInt32(ptr);
            for (uint32_t j = 0; j < varNamesCount; j++) {
                BufferUtils::readPrefixedUtf16String(ptr);
            }
        }
        
        if (found) {
            return resultStr;
        }
    }
    
    spdlog::warn("Token {} not found in Table {} (Scanned {} entries)", tokenId, tableId, nbEntries);
    return QString();
}

} // namespace lotro::dat
