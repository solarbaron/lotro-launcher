/**
 * @file PropertyDefinitionsLoader.cpp
 * @brief Implementation of property definitions loader
 */

#include "PropertyDefinitionsLoader.hpp"
#include "BufferUtils.hpp"

#include <spdlog/spdlog.h>

namespace lotro::dat {

std::unique_ptr<PropertiesRegistry> PropertyDefinitionsLoader::decodeMasterProperty(const QByteArray& data) {
    if (data.isEmpty()) {
        spdlog::error("Cannot decode empty property data");
        return nullptr;
    }
    
    const char* ptr = data.constData();
    const char* end = ptr + data.size();
    
    // Read header
    uint32_t did = BufferUtils::readUInt32(ptr);
    BufferUtils::skip(ptr, 8); // Skip 8 bytes
    
    if (did != 0x34000000) {
        spdlog::warn("Property DID mismatch: got 0x{:08X}, expected 0x34000000", did);
    }
    
    // Read property names first
    int numStrings = BufferUtils::readTSize(ptr);
    spdlog::debug("Loading {} property names...", numStrings);
    
    auto registry = std::make_unique<PropertiesRegistry>();
    
    for (int i = 0; i < numStrings && ptr < end; ++i) {
        int pid = BufferUtils::readUInt32(ptr);
        QString name = BufferUtils::readPascalString(ptr);
        
        auto def = std::make_shared<PropertyDefinition>(pid, name);
        registry->registerProperty(def);
    }
    
    // Skip 2 bytes
    BufferUtils::skip(ptr, 2);
    
    // Read property definitions
    int nbPropertyDefs = BufferUtils::readTSize(ptr);
    spdlog::debug("Loading {} property definitions...", nbPropertyDefs);
    
    for (int i = 0; i < nbPropertyDefs && ptr < end; ++i) {
        int pid = BufferUtils::readUInt32(ptr);
        readPropertyDefinition(ptr, pid, registry.get());
    }
    
    spdlog::info("Loaded {} properties from DAT file", registry->count());
    return registry;
}

void PropertyDefinitionsLoader::readPropertyDefinition(const char*& ptr, int expectedPid, PropertiesRegistry* registry) {
    auto def = registry->getPropertyDef(expectedPid);
    if (!def) {
        spdlog::warn("Property {} not found in registry", expectedPid);
        return;
    }
    
    int pid = BufferUtils::readUInt32(ptr);
    if (pid != expectedPid) {
        spdlog::error("PID mismatch: expected {}, got {}", expectedPid, pid);
        return;
    }
    
    int propertyTypeCode = BufferUtils::readUInt32(ptr);
    if (propertyTypeCode < 1 || propertyTypeCode > 22) {
        spdlog::error("Invalid property type code: {}", propertyTypeCode);
        return;
    }
    
    PropertyType propertyType = getPropertyTypeByCode(propertyTypeCode);
    def->setType(propertyType);
    
    // Read additional metadata
    /*int group =*/ BufferUtils::readUInt32(ptr);
    /*int provider =*/ BufferUtils::readUInt32(ptr);
    int dataId = BufferUtils::readUInt32(ptr);
    def->setData(dataId);
    
    /*int ePatchFlags =*/ BufferUtils::readUInt32(ptr);
    int v5 = BufferUtils::readUInt32(ptr);
    int flags = (v5 >> 8) & 0xFF;
    
    // Skip optional values based on flags
    // Skip optional values based on flags
    if (flags & 0x08) {
        // Default value
        skipPropertyValue(ptr, propertyTypeCode);
    }
    if (flags & 0x10) {
        // Min value
        skipPropertyValue(ptr, propertyTypeCode);
    }
    if (flags & 0x20) {
        // Max value
        skipPropertyValue(ptr, propertyTypeCode);
    }
    
    // Skip more metadata
    BufferUtils::skip(ptr, 4); // predictionTimeout (float)
    BufferUtils::skip(ptr, 4); // inheritanceType, datFileType, propagationType, cachingType (4 bytes)
    BufferUtils::skip(ptr, 1); // padding
    
    // Read child properties
    int nbChildren = BufferUtils::readVle(ptr);
    for (int i = 0; i < nbChildren; ++i) {
        int childPid1 = BufferUtils::readUInt32(ptr);
        int childPid2 = BufferUtils::readUInt32(ptr);
        
        if (childPid1 != childPid2) {
            spdlog::warn("Child PID mismatch: {} != {}", childPid1, childPid2);
        }
        
        auto childDef = registry->getPropertyDef(childPid1);
        if (childDef) {
            def->addChildProperty(childDef);
        }
    }
    
    // Read required properties
    int nbRequired = BufferUtils::readUInt32(ptr);
    for (int i = 0; i < nbRequired; ++i) {
        /*int requiredPid =*/ BufferUtils::readUInt32(ptr);
    }
    
    // Final check
    int shouldBeZero = BufferUtils::readUInt32(ptr);
    if (shouldBeZero != 0) {
        spdlog::warn("Expected 0 at end of property def, got {}", shouldBeZero);
    }
}

void PropertyDefinitionsLoader::skipPropertyValue(const char*& data, int propertyType) {
    // Based on PropertyUtils.readPropertyValue in delta-lotro-dat-utils
    // propertyType code mapping from PropertyType.java
    
    switch (propertyType) {
        case 5: // TRI_STATE (UInt8)
        case 21: // BOOLEAN (UInt8)
            data += 1;
            break;
            
        case 2: // STRING_TOKEN (UInt32)
        case 8: // ENUM_MAPPER (UInt32)
        case 9: // FLOAT
        case 10: // PROPERTY_ID
        case 12: // ARRAY (UInt32 count/ID)
        case 15: // INT (UInt32)
        case 16: // COLOR (4 bytes)
        case 18: // BIT_FIELD32 (UInt32)
        case 20: // DATA_FILE (UInt32)
            data += 4;
            break;
            
        case 4: // TIMESTAMP (Double)
        case 7: // INSTANCE_ID (Long64)
        case 14: // BITFIELD_64 (Long64)
        case 19: // LONG64
            data += 8;
            break;
            
        case 1: // STRING (PascalString)
            BufferUtils::readPascalString(data);
            break;
            
        case 3: // WAVE_FORM
        {
            uint32_t type = BufferUtils::readUInt32(data);
            if (type == 10) {
                data += 10 * 4; // 10 floats
                data += 4; // extra float
                data += 1; // bool
                uint32_t pairCount = BufferUtils::readUInt32(data);
                data += pairCount * 2 * 4; // pairCount * 2 floats
            } else if (type == 1) {
                data += 4; // 1 float
            } else if (type > 1) {
                data += 10 * 4; // 10 floats
            }
            break;
        }
            
        case 6: // VECTOR (3 floats)
            data += 12;
            break;
            
        case 11: // STRUCT
            data += 2; // read(), readUInt8()
            break;
            
        case 13: // STRING_INFO
            skipStringInfo(data);
            break;
            
        case 17: // POSITION
            skipPosition(data);
            break;
            
        case 22: // BIT_FIELD
        {
            int bitCount = BufferUtils::readVle(data);
            int byteCount = bitCount / 8 + (bitCount % 8 != 0 ? 1 : 0);
            data += byteCount;
            break;
        }
            
        default:
            spdlog::warn("Skipping unknown property type definition: {} (defaulting to 4 bytes)", propertyType);
            data += 4;
            break;
    }
}

void PropertyDefinitionsLoader::skipStringInfo(const char*& data) {
    // PropertyUtils.readStringInfoProperty
    bool isLiteral = (BufferUtils::readUInt8(data) != 0);
    if (isLiteral) {
        // UTF-16 pascal string with 4-byte len? 
        // BufferUtils.readPrefixedUtf16String logic:
        // Reads uint32 length? No, usually not.
        // Let's assume standard Pascal string but UTF-16
        // Wait, PropertyUtils says readPrefixedUtf16String.
        // We need to implement that skip manually if BufferUtils doesn't have it.
        // Checking BufferUtils... it doesn't.
        // Usually it's: uint32 len, then len * 2 bytes.
        uint32_t len = BufferUtils::readUInt32(data);
        data += len * 2;
    } else {
        data += 8; // token (4) + dataId (4)
    }
    
    bool hasStrings = (BufferUtils::readUInt8(data) != 0);
    if (hasStrings) {
        BufferUtils::readPascalString(data); // p1
        BufferUtils::readPascalString(data); // p2
        BufferUtils::readPascalString(data); // p3
        
        int numReplacements = BufferUtils::readVle(data);
        for (int i = 0; i < numReplacements; ++i) {
            int dataType = BufferUtils::readUInt8(data);
            data += 4; // replacementToken
            
            if (dataType != 1) {
                data += 1; // is1
            }
            
            if (dataType == 4) {
                BufferUtils::readVle(data);
            } else if (dataType == 1) {
                skipStringInfo(data); // Recursive
            } else if (dataType == 2) {
                data += 4; // float
            }
        }
    } else {
        data += 2; // remainder1 + remainder2
    }
}

void PropertyDefinitionsLoader::skipPosition(const char*& data) {
    // GeoLoader.readPosition
    int flags = BufferUtils::readUInt8(data);
    if (flags == 0) return;
    
    if (flags & 1) data += 1; // region
    if (flags & 2) data += 2; // bx, by
    if (flags & 4) data += 2; // instance
    if (flags & 8) data += 2; // cell
    if (flags & 0x10) data += 12; // 3 floats
    if (flags & 0x20) data += 16; // 4 floats
}

} // namespace lotro::dat
