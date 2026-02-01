/**
 * LOTRO Launcher - Memory Configuration
 * 
 * Memory addresses and configuration for reading LOTRO client data.
 * Based on decompiled lotro-memory library patterns.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace lotro {

/**
 * Memory configuration for LOTRO client
 * 
 * Contains offsets from module base address for various data structures.
 * These values are derived from the lotro-memory library.
 */
struct LotroMemoryConfig {
    // Whether this config is for 64-bit client
    bool is64Bit = true;
    
    // Base address of lotroclient module
    uint64_t baseAddress = 0;
    
    // Offsets from base address
    uint64_t entitiesTableOffset = 0;
    uint64_t referencesTableOffset = 0;
    uint64_t clientDataOffset = 0;
    uint64_t accountDataOffset = 0;
    uint64_t storageDataOffset = 0;
    
    // Size parameters
    int pointerSize = 8;
    int intSize = 8;
    int intKeySizeForMaps = 8;
    int bucketSize = 24;
    int worldEntityOffset = 16;
    
    /**
     * Get configuration for 64-bit client
     */
    static LotroMemoryConfig config64Bit() {
        LotroMemoryConfig config;
        config.is64Bit = true;
        config.entitiesTableOffset = 0x1e03738;
        config.referencesTableOffset = 0x1dff3d8;
        config.clientDataOffset = 0x1dfedc8;
        config.accountDataOffset = 0x1dfedc8;
        config.storageDataOffset = 0x1e1b060;
        config.pointerSize = 8;
        config.intSize = 8;
        config.intKeySizeForMaps = 8;
        config.bucketSize = 24;
        config.worldEntityOffset = 16;
        return config;
    }
    
    /**
     * Get configuration for 32-bit client
     */
    static LotroMemoryConfig config32Bit() {
        LotroMemoryConfig config;
        config.is64Bit = false;
        config.entitiesTableOffset = 0x1b03f08;
        config.referencesTableOffset = 0x1d2e5f4;
        config.clientDataOffset = 0x1d2e214;
        config.accountDataOffset = 0x1d2e214;
        config.storageDataOffset = 0x1b192c8;
        config.pointerSize = 4;
        config.intSize = 4;
        config.intKeySizeForMaps = 4;
        config.bucketSize = 24;
        config.worldEntityOffset = 16;
        return config;
    }
    
    /**
     * Get absolute address for entities table
     */
    uint64_t entitiesTableAddress() const {
        return baseAddress + entitiesTableOffset;
    }
    
    /**
     * Get absolute address for client data
     */
    uint64_t clientDataAddress() const {
        return baseAddress + clientDataOffset;
    }
    
    /**
     * Get absolute address for account data
     */
    uint64_t accountDataAddress() const {
        return baseAddress + accountDataOffset;
    }
    
    /**
     * Get absolute address for storage data
     */
    uint64_t storageDataAddress() const {
        return baseAddress + storageDataOffset;
    }
};

} // namespace lotro
