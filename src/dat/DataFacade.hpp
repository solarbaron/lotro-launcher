/**
 * @file DataFacade.hpp
 * @brief High-level interface for accessing LOTRO DAT file data
 */

#pragma once

#include "DatArchive.hpp"
#include "PropertiesRegistry.hpp"

#include <QString>
#include <vector>
#include <memory>

namespace lotro::dat {

/**
 * @class DataFacade
 * @brief Main interface for reading LOTRO DAT files
 * 
 * This class provides a high-level interface to load and access data
 * from LOTRO's DAT files. It handles:
 * - Opening the main DAT files (client_local_*.dat)
 * - Loading and caching the properties registry
 * - Looking up data by ID
 * 
 * Usage:
 * @code
 * DataFacade facade("/path/to/lotro");
 * auto registry = facade.getPropertiesRegistry();
 * int nameId = registry->getPropertyId("Name");
 * @endcode
 */
class DataFacade {
public:
    /**
     * @brief Construct a DataFacade for the given game installation
     * @param gamePath Path to the LOTRO installation directory
     */
    explicit DataFacade(const QString& gamePath);
    
    /**
     * @brief Destructor - closes all open DAT files
     */
    ~DataFacade();
    
    /**
     * @brief Initialize and open DAT files
     * @return true if at least one DAT file was opened successfully
     */
    bool initialize();
    
    /**
     * @brief Get the properties registry
     * 
     * Loads the registry lazily on first access from data ID 0x34000000.
     * @return The properties registry, or nullptr if loading failed
     */
    PropertiesRegistry* getPropertiesRegistry();
    
    /**
     * @brief Load raw data by ID
     * @param dataId The data ID to load
     * @return The raw data, or empty array if not found
     */
    QByteArray loadData(uint64_t dataId);
    
    /**
     * @brief Resolve a string from a string table
     * @param tableId The string table ID
     * @param tokenId The token ID within the table
     * @return The resolved string, or empty if not found
     */
    QString resolveString(uint32_t tableId, uint32_t tokenId);
    
    /**
     * @brief Check if the facade is initialized
     */
    bool isInitialized() const { return !m_archives.empty(); }
    
    /**
     * @brief Get the game installation path
     */
    QString gamePath() const { return m_gamePath; }
    
    /**
     * @brief Clean up and close all resources
     */
    void dispose();
    
private:
    // Find and open all DAT files in the game directory
    bool openDatFiles();
    
private:
    QString m_gamePath;
    std::vector<std::unique_ptr<DatArchive>> m_archives;
    std::unique_ptr<PropertiesRegistry> m_propertiesRegistry;
    
    // Data ID for the master properties definition
    static constexpr uint64_t PROPERTIES_DATA_ID = 0x34000000;
};

} // namespace lotro::dat
