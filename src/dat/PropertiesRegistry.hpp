/**
 * @file PropertiesRegistry.hpp
 * @brief Registry mapping property IDs to names and vice versa
 */

#pragma once

#include "PropertyDefinition.hpp"
#include <QMap>
#include <QList>

namespace lotro::dat {

/**
 * @class PropertiesRegistry
 * @brief Provides bidirectional mapping between property IDs and names
 * 
 * This is the core class for property resolution. It maps:
 * - Property ID (integer) → Property Name (string)
 * - Property Name (string) → Property ID (integer)
 */
class PropertiesRegistry {
public:
    PropertiesRegistry() = default;
    
    /**
     * @brief Register a property definition
     */
    void registerProperty(PropertyDefinitionPtr def) {
        if (!def) return;
        m_propertiesById[def->propertyId()] = def;
        m_propertiesByName[def->name()] = def;
    }
    
    /**
     * @brief Get a property by its ID
     * @return The property definition, or nullptr if not found
     */
    PropertyDefinitionPtr getPropertyDef(int propertyId) const {
        return m_propertiesById.value(propertyId, nullptr);
    }
    
    /**
     * @brief Get a property by its name
     * @return The property definition, or nullptr if not found
     */
    PropertyDefinitionPtr getPropertyDefByName(const QString& name) const {
        return m_propertiesByName.value(name, nullptr);
    }
    
    /**
     * @brief Get the property ID for a given name
     * @return The property ID, or -1 if not found
     */
    int getPropertyId(const QString& name) const {
        auto def = getPropertyDefByName(name);
        return def ? def->propertyId() : -1;
    }
    
    /**
     * @brief Get the property name for a given ID
     * @return The property name, or empty string if not found
     */
    QString getPropertyName(int propertyId) const {
        auto def = getPropertyDef(propertyId);
        return def ? def->name() : QString();
    }
    
    /**
     * @brief Get all property IDs
     */
    QList<int> propertyIds() const {
        return m_propertiesById.keys();
    }
    
    /**
     * @brief Get the number of registered properties
     */
    int count() const {
        return m_propertiesById.size();
    }
    
private:
    QMap<int, PropertyDefinitionPtr> m_propertiesById;
    QMap<QString, PropertyDefinitionPtr> m_propertiesByName;
};

} // namespace lotro::dat
