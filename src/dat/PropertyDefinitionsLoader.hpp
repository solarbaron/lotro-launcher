/**
 * @file PropertyDefinitionsLoader.hpp
 * @brief Loads property definitions from DAT file data
 */

#pragma once

#include "PropertiesRegistry.hpp"
#include <QByteArray>
#include <memory>

namespace lotro::dat {

/**
 * @class PropertyDefinitionsLoader
 * @brief Parses the master property definitions from DAT file data
 * 
 * The property definitions are stored at data ID 0x34000000.
 * This class decodes the binary format into a PropertiesRegistry.
 */
class PropertyDefinitionsLoader {
public:
    /**
     * @brief Decode the master property definitions
     * @param data Raw data from data ID 0x34000000
     * @return The populated properties registry
     */
    std::unique_ptr<PropertiesRegistry> decodeMasterProperty(const QByteArray& data);
    
private:
    // Read a single property definition
    void readPropertyDefinition(const char*& ptr, int expectedPid, PropertiesRegistry* registry);
    
    // Skip over a property value based on type
    void skipPropertyValue(const char*& ptr, int propertyType);
    
    // Helper to skip StringInfo structure
    void skipStringInfo(const char*& ptr);
    
    // Helper to skip Position structure
    void skipPosition(const char*& ptr);
};

} // namespace lotro::dat
