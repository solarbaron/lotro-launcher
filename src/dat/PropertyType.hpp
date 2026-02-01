/**
 * @file PropertyType.hpp
 * @brief Enumeration of property types in LOTRO DAT files
 */

#pragma once

#include <cstdint>

namespace lotro::dat {

/**
 * @enum PropertyType
 * @brief Types of properties that can be stored in LOTRO data
 * Based on PropertyUtils in delta-lotro-dat-utils
 */
enum class PropertyType : int {
    UNKNOWN = 0,
    STRING = 1,
    STRING_TOKEN = 2,
    WAVE_FORM = 3,
    TIMESTAMP = 4,
    TRI_STATE = 5,
    VECTOR = 6,
    INSTANCE_ID = 7,
    ENUM_MAPPER = 8,
    FLOAT = 9,
    PROPERTY_ID = 10,
    STRUCT = 11,
    ARRAY = 12,
    STRING_INFO = 13,
    BITFIELD_64 = 14,
    INT = 15,
    COLOR = 16,
    POSITION = 17,
    BIT_FIELD32 = 18,
    LONG64 = 19,
    DATA_FILE = 20,
    BOOLEAN = 21,
    BIT_FIELD = 22
};

/**
 * @brief Get property type from LOTRO type code
 */
inline PropertyType getPropertyTypeByCode(int code) {
    if (code >= 0 && code <= 22) {
        return static_cast<PropertyType>(code);
    }
    return PropertyType::UNKNOWN;
}

/**
 * @brief Get type code from property type
 */
inline int getCodeFromPropertyType(PropertyType type) {
    return static_cast<int>(type);
}

} // namespace lotro::dat
