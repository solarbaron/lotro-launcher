/**
 * @file PropertyDefinition.hpp
 * @brief Definition of a property in the LOTRO property system
 */

#pragma once

#include "PropertyType.hpp"
#include <QString>
#include <QVector>
#include <memory>

namespace lotro::dat {

class PropertyDefinition;
using PropertyDefinitionPtr = std::shared_ptr<PropertyDefinition>;

/**
 * @class PropertyDefinition
 * @brief Defines a property with its ID, name, and type
 * 
 * Properties are the fundamental unit of data in LOTRO. Each property
 * has a unique numeric ID and a string name. This class provides the
 * mapping between the two.
 */
class PropertyDefinition {
public:
    PropertyDefinition(int propertyId, const QString& name, PropertyType type = PropertyType::UNKNOWN)
        : m_propertyId(propertyId)
        , m_name(name)
        , m_type(type)
    {}
    
    int propertyId() const { return m_propertyId; }
    QString name() const { return m_name; }
    PropertyType type() const { return m_type; }
    
    void setType(PropertyType type) { m_type = type; }
    
    int data() const { return m_data; }
    void setData(int data) { m_data = data; }
    
    void addChildProperty(PropertyDefinitionPtr child) {
        if (child) {
            m_children.append(child);
        }
    }
    
    bool hasChildren() const { return !m_children.isEmpty(); }
    const QVector<PropertyDefinitionPtr>& children() const { return m_children; }
    
    QString toString() const {
        return QString("Property: %1, ID=%2, type=%3")
            .arg(m_name)
            .arg(m_propertyId)
            .arg(static_cast<int>(m_type));
    }
    
private:
    int m_propertyId;
    QString m_name;
    PropertyType m_type;
    int m_data = 0;
    QVector<PropertyDefinitionPtr> m_children;
};

} // namespace lotro::dat
