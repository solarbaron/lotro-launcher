/**
 * LOTRO Launcher - Character Extractor Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CharacterExtractor.hpp"
#include "PatternScanner.hpp"

#include <spdlog/spdlog.h>
#include <map>
#include <set>
#include <algorithm>
#include <cstring>

namespace lotro {

CharacterExtractor::CharacterExtractor(const QString& gamePath)
    : m_memory(std::make_unique<ProcessMemory>())
    , m_gamePath(gamePath)
{
    // Initialize DataFacade if game path is provided
    if (!m_gamePath.isEmpty()) {
        m_datFacade = std::make_unique<dat::DataFacade>(m_gamePath);
        if (m_datFacade->initialize()) {
            // Resolve property IDs
            if (auto* registry = m_datFacade->getPropertiesRegistry()) {
                // Helper: try multiple candidate names, return first resolved ID
                auto resolveProperty = [registry](const std::vector<QString>& candidates, const QString& description) -> int {
                    for (const auto& name : candidates) {
                        int id = registry->getPropertyId(name);
                        if (id != -1) {
                            spdlog::info("  {} resolved: '{}' = {}", description.toStdString(), name.toStdString(), id);
                            return id;
                        }
                    }
                    // Log all candidates that were tried
                    spdlog::warn("  {} NOT resolved. Tried:", description.toStdString());
                    for (const auto& name : candidates) {
                        spdlog::warn("    - '{}'", name.toStdString());
                    }
                    return -1;
                };
                
                // Helper: search registry for properties matching a keyword and log them
                auto searchAndLog = [registry](const QString& keyword, int maxResults = 20) {
                    auto props = registry->searchProperties(keyword, maxResults);
                    if (!props.isEmpty()) {
                        spdlog::info("  Search '{}': {} results", keyword.toStdString(), props.size());
                        for (const auto& p : props) {
                            spdlog::info("    {} (ID={}, type={})", p->name().toStdString(), p->propertyId(), static_cast<int>(p->type()));
                        }
                    }
                };
                
                spdlog::info("=== Resolving Property IDs ===");
                
                m_namePropertyId = resolveProperty({"Name"}, "Name");
                m_levelPropertyId = resolveProperty({"Advancement_Level", "Level", "AdvTable_Level"}, "Level");
                m_accountNamePropertyId = resolveProperty({"Account_BillingName", "Account_Name"}, "AccountName");
                m_subscriptionKeyPropertyId = resolveProperty({"Account_GameAccountName"}, "SubscriptionKey");
                m_accountTypePropertyId = resolveProperty({"Billing@Player.AccountType", "Account_Type"}, "AccountType");
                
                // Class: Agent_Class is the correct property in most DAT versions
                m_classPropertyId = resolveProperty({
                    "Agent_Class", "AdvTable_Class", "Class", "CharacterClass"
                }, "Class");
                
                // Race: Agent_Species is the correct property for race (confirmed from entity property discovery).
                // AdvTable_Race resolves from DAT but is NOT present on the player entity.
                m_racePropertyId = resolveProperty({
                    "Agent_Species", "Agent_Race", "AdvTable_Race",
                    "AdvTable_Species", "Race", "Species",
                    "CharacterRace", "AdvTable_Genus", "Agent_Genus", "Genus"
                }, "Race");
                
                // Morale (health vitals)
                m_maxMoralePropertyId = resolveProperty({
                    "Health_MaxLevel", "Vital_MaxMorale", "Morale_Max",
                    "MaxMorale", "Vital_MaxHealth"
                }, "MaxMorale");
                m_currentMoralePropertyId = resolveProperty({
                    "Health_CurrentLevel", "Vital_CurrentMorale", "Morale_Current",
                    "CurrentMorale", "Vital_CurrentHealth"
                }, "CurrentMorale");
                
                // Power
                m_maxPowerPropertyId = resolveProperty({
                    "Power_MaxLevel", "Vital_MaxPower", "Power_Max", "MaxPower"
                }, "MaxPower");
                m_currentPowerPropertyId = resolveProperty({
                    "Power_CurrentLevel", "Vital_CurrentPower", "Power_Current", "CurrentPower"
                }, "CurrentPower");
                
                // Money
                m_moneyPropertyId = resolveProperty({
                    "Currency_Amount", "Inventory_Money", "Money", "Gold_Amount",
                    "Wallet_Money", "Currency_Total"
                }, "Money");
                
                // Destiny Points
                m_destinyPointsPropertyId = resolveProperty({
                    "MonsterPlay_SessionPoints", "DestinyPoints", "Destiny_Points",
                    "Account_DestinyPoints"
                }, "DestinyPoints");
                
                spdlog::info("=== Property Resolution Summary ===");
                spdlog::info("  Name={}, Level={}, Class={}, Race={}", 
                             m_namePropertyId, m_levelPropertyId, m_classPropertyId, m_racePropertyId);
                spdlog::info("  CurrentMorale={}, CurrentPower={}",
                             m_currentMoralePropertyId, m_currentPowerPropertyId);
                spdlog::info("  Money={}, DestinyPoints={}",
                             m_moneyPropertyId, m_destinyPointsPropertyId);
                
                // If any critical properties failed, do broad searches to discover names
                if (m_racePropertyId == -1) {
                    spdlog::info("=== Race Property Discovery ===");
                    searchAndLog("Race", 30);
                    searchAndLog("Species", 20);
                    searchAndLog("Genus", 20);
                    searchAndLog("Agent_", 30);
                }
                
                if (m_maxMoralePropertyId == -1 || m_currentMoralePropertyId == -1) {
                    spdlog::info("=== Morale Property Discovery ===");
                    searchAndLog("Morale", 30);
                    searchAndLog("Health", 30);
                    searchAndLog("Vital", 30);
                }
                if (m_maxPowerPropertyId == -1 || m_currentPowerPropertyId == -1) {
                    spdlog::info("=== Power Property Discovery ===");
                    searchAndLog("Power", 30);
                }
                if (m_moneyPropertyId == -1) {
                    spdlog::info("=== Money Property Discovery ===");
                    searchAndLog("Money", 20);
                    searchAndLog("Currency", 20);
                    searchAndLog("Gold", 20);
                    searchAndLog("Wallet", 20);
                }

            }
        } else {
            spdlog::warn("Failed to initialize DAT file access");
        }
    }
}

CharacterExtractor::~CharacterExtractor() {
    disconnect();
}

bool CharacterExtractor::connect() {
    // Find LOTRO client process
    auto clientInfo = ProcessMemory::findLotroClient();
    if (!clientInfo) {
        m_lastError = "LOTRO client not found. Make sure the game is running.";
        spdlog::warn("{}", m_lastError.toStdString());
        return false;
    }
    
    spdlog::info("Found LOTRO client: {} (PID: {})", 
                 clientInfo->name, clientInfo->pid);
    
    // Open process
    if (!m_memory->open(clientInfo->pid)) {
        m_lastError = "Failed to open LOTRO process. Try running with administrator/root privileges.";
        return false;
    }
    
    // Set configuration based on architecture
    if (clientInfo->is64Bit) {
        m_config = LotroMemoryConfig::config64Bit();
    } else {
        m_config = LotroMemoryConfig::config32Bit();
    }
    
    // Get module base address
    std::string moduleName = clientInfo->is64Bit ? "lotroclient64.exe" : "lotroclient.exe";
    auto baseAddr = m_memory->getModuleBaseAddress(moduleName);
    if (!baseAddr) {
        m_lastError = QString("Failed to find %1 module").arg(QString::fromStdString(moduleName));
        disconnect();
        return false;
    }
    
    // Adjust base address (as in the Java code: base + 4096 - 1024)
    // m_config.baseAddress = *baseAddr + 4096 - 1024;
    // NOTE: Scanning patterns makes base address adjustment less critical/obsolete
    // but we still keep it as reference. Linux base might be fine as is.
    m_config.baseAddress = *baseAddr;
    
    spdlog::info("Connected to LOTRO client, base address: 0x{:X}", m_config.baseAddress);
    
    // Scan for patterns
    if (!scanPatterns()) {
        spdlog::warn("Pattern scanning failed, falling back to hardcoded offsets");
        // Fallback or fail? Fallback for now in case patterns fail but hardcoded works (unlikely if outdated)
    }
    
    // Read initial client data
    if (!readClientData()) {
        spdlog::warn("Could not read initial client data");
    }
    
    return true;
}

void CharacterExtractor::disconnect() {
    m_memory->close();
    m_server.clear();
    m_account.clear();
}

bool CharacterExtractor::isConnected() const {
    return m_memory->isOpen();
}

bool CharacterExtractor::readClientData() {
    // Try to read client data structure
    // The client data contains server name, language, etc.
    
    uint64_t clientDataAddr = m_config.clientDataAddress();
    spdlog::debug("Reading client data from 0x{:X}", clientDataAddr);
    
    // 1. Dereference global pointer to get ClientData struct address
    auto clientDataStructPtr = m_memory->readPointer(clientDataAddr);
    if (!clientDataStructPtr || *clientDataStructPtr == 0) {
        spdlog::debug("Client data struct pointer is null");
        return false;
    }
    
    uint64_t dataStructAddr = *clientDataStructPtr;
    spdlog::debug("ClientData struct at 0x{:X}", dataStructAddr);
    
    // 2. Read Server Name
    // It's a pointer at offset 312 (64-bit) or 180 (32-bit)
    uint64_t serverOffset = m_config.is64Bit ? 312 : 180;
    auto serverNamePtr = m_memory->readPointer(dataStructAddr + serverOffset);
    
    if (serverNamePtr && *serverNamePtr != 0) {
        // Read UTF-16 string (lotro uses wide strings for names usually, java used readNullTerminatedUTF16String)
        // But readString in ProcessMemory reads narrow string.
        // Let's check ProcessMemory API for wide string or check if string is actually wide.
        // ClientDataController uses readNullTerminatedUTF16String.
        
        auto serverNameWide = m_memory->readWideString(*serverNamePtr, 64);
        if (serverNameWide && !serverNameWide->empty()) {
            // Convert wide string to QString
            m_server = QString::fromStdWString(*serverNameWide);
            
            // Java code removes "]" prefix logic: index of 93 (']')
             int idx = m_server.indexOf(']');
             if (idx != -1) {
                 m_server = m_server.mid(idx + 1).trimmed();
             }
            
            spdlog::info("Server: {}", m_server.toStdString());
        }
    }
    
    return true;
}

std::optional<QString> CharacterExtractor::getServerName() {
    if (m_server.isEmpty()) {
        readClientData();
    }
    if (m_server.isEmpty()) {
        return std::nullopt;
    }
    return m_server;
}

std::optional<QString> CharacterExtractor::getAccountName() {
    if (!m_account.isEmpty()) {
        return m_account;
    }
    
    // Account data is at the same offset as client data
    if (m_accountNamePropertyId != -1) {
        auto billingName = readAccountStringProperty(m_accountNamePropertyId);
        auto subKey = readAccountStringProperty(m_subscriptionKeyPropertyId);
        
        if (billingName) {
            m_account = *billingName;
            if (subKey && !subKey->isEmpty()) {
                m_account += QString(" (%1)").arg(*subKey);
            }
            return m_account;
        }
    }
    
    return std::nullopt;
}

std::optional<uint64_t> CharacterExtractor::readHashtableValue(uint64_t hashtableBaseAddr, uint32_t propId) {
    // 64-bit hashtable layout:
    // +16: Buckets Array Pointer
    // +32: NbBuckets (int)
    
    // Read header (enough for buckets ptr and nbBuckets)
    auto headBuf = m_memory->readMemory(hashtableBaseAddr, 48);
    if (!headBuf) return std::nullopt;
    
    // TODO: Handle 32-bit offsets if needed (for now focused on 64-bit)
    uint64_t bucketsPtr = headBuf->readPointer(16, true);
    uint32_t nbBuckets = headBuf->read<uint32_t>(32);
    
    if (bucketsPtr == 0 || nbBuckets == 0 || nbBuckets > 100000) return std::nullopt;
    
    uint32_t bucketIdx = propId % nbBuckets;
    
    // Read bucket head pointer
    auto bucketHeadPtrBuf = m_memory->readPointer(bucketsPtr + bucketIdx * 8);
    if (!bucketHeadPtrBuf) return std::nullopt;
    
    uint64_t nodePtr = *bucketHeadPtrBuf;
    
    // Traverse bucket (hashed lookup)
    int iterations = 0;
    while (nodePtr != 0 && iterations++ < 50) {
        auto nodeBuf = m_memory->readMemory(nodePtr, 32);
        if (!nodeBuf) break;
        
        // ID is at 0 (aligned to 8 bytes in 64-bit map node)
        uint32_t id = nodeBuf->read<uint32_t>(0);
        if (id == propId) {
            // Value is at offset 24
            return nodeBuf->read<uint64_t>(24);
        }
        
        // Next pointer at offset 8
        nodePtr = nodeBuf->readPointer(8, true);
    }
    
    // Hashed bucket failed — try linear scan of ALL buckets 
    // This handles cases where the hash function differs from simple modulo
    for (uint32_t i = 0; i < nbBuckets && i < 2048; i++) {
        if (i == bucketIdx) continue; // Already checked
        
        auto bpBuf = m_memory->readPointer(bucketsPtr + i * 8);
        if (!bpBuf) continue;
        
        uint64_t np = *bpBuf;
        int iter2 = 0;
        while (np != 0 && iter2++ < 50) {
            auto nb = m_memory->readMemory(np, 32);
            if (!nb) break;
            
            uint32_t id = nb->read<uint32_t>(0);
            if (id == propId) {
                spdlog::info("Property {} found in bucket {} (expected bucket {} with {} total buckets)",
                             propId, i, bucketIdx, nbBuckets);
                return nb->read<uint64_t>(24);
            }
            np = nb->readPointer(8, true);
        }
    }
    
    return std::nullopt;
}

std::optional<uint64_t> CharacterExtractor::readAccountPropertyValue(uint32_t propId) {
    uint64_t clientDataAddr = m_config.clientDataAddress();
    auto clientDataPtr = m_memory->readPointer(clientDataAddr);
    if (!clientDataPtr || *clientDataPtr == 0) return std::nullopt;
    
    uint64_t dataStructAddr = *clientDataPtr;
    
    // Account Property Provider pointer
    // 64-bit: 408
    uint64_t accProviderOffset = m_config.is64Bit ? 408 : 232;
    auto accProviderPtr = m_memory->readPointer(dataStructAddr + accProviderOffset);
    if (!accProviderPtr || *accProviderPtr == 0) return std::nullopt;
    
    // Hashtable base
    // 64-bit: 184
    uint64_t hashTableOffset = m_config.is64Bit ? 184 : 108;
    
    return readHashtableValue(*accProviderPtr + hashTableOffset, propId);
}

std::optional<QString> CharacterExtractor::readAccountStringProperty(uint32_t propId) {
    // Re-use logic from readStringProperty but calling readAccountPropertyValue
    // And assuming we have the property ID resolved
    auto val = readAccountPropertyValue(propId);
    if (!val) return std::nullopt;
    
    uint64_t valPtr = *val;
    if (valPtr < 0x10000 || valPtr > 0x7FFFFFFFFFFF) return std::nullopt;
    
    // Read string using same logic as normal properties (StringPropValue)
    // Offset 16 is string pointer
    auto objBuf = m_memory->readMemory(valPtr, 32);
    if (!objBuf) return std::nullopt;
    
    uint64_t strPtr = objBuf->readPointer(16, true);
    if (strPtr > 0x10000 && strPtr < 0x7FFFFFFFFFFF) {
        auto s = m_memory->readString(strPtr, 128);
        if (s && !s->empty()) return QString::fromLatin1(s->c_str());
        
        auto ws = m_memory->readWideString(strPtr, 128);
        if (ws && !ws->empty()) return QString::fromStdWString(*ws);
    }
    
    return std::nullopt;
}

std::optional<int> CharacterExtractor::readAccountIntProperty(uint32_t propId) {
    auto val = readAccountPropertyValue(propId);
    if (!val) return std::nullopt;
    return static_cast<int>(*val);
}

std::optional<uint64_t> CharacterExtractor::readStoragePropertyValue(uint32_t propId) {
    uint64_t storageAddr = m_config.storageDataAddress();
    auto storagePtr = m_memory->readPointer(storageAddr);
    if (!storagePtr || *storagePtr == 0) return std::nullopt;
    
    uint64_t dataStructAddr = *storagePtr;
    
    // The storage data has a property provider similar to account data
    // Try the same layout as entity EPP (hashtable at offset 56 from provider)
    // The storage data structure may have its own provider at various offsets
    
    // Try reading as entity-style EPP
    // Storage data -> property provider -> hashtable
    uint64_t eppOffset = m_config.is64Bit ? 192 : 108;
    auto storeBuf = m_memory->readMemory(dataStructAddr, 256);
    if (!storeBuf) return std::nullopt;
    
    uint64_t eppPtr = storeBuf->readPointer(eppOffset, true);
    if (eppPtr != 0) {
        uint64_t hashTableOffset = m_config.is64Bit ? 56 : 32;
        auto val = readHashtableValue(eppPtr + hashTableOffset, propId);
        if (val) return val;
    }
    
    // Try direct hashtable at common offsets
    for (uint64_t offset : {56UL, 72UL, 184UL, 192UL, 208UL}) {
        auto val = readHashtableValue(dataStructAddr + offset, propId);
        if (val) {
            spdlog::info("Found storage property {} at structure offset {}", propId, offset);
            return val;
        }
    }
    
    return std::nullopt;
}
std::optional<uint64_t> CharacterExtractor::findPlayerEntity() {
    // The entities table contains all game entities including the player
    // We need to traverse the table to find the player entity
    
    uint64_t entitiesTableAddr = m_config.entitiesTableAddress();
    spdlog::debug("Searching entities table at 0x{:X}", entitiesTableAddr);
    

    // Scan entities to find the main player
    // Strategy: Player usually has the most properties.
    // We also look for specific string properties (Name).
    
    struct EntityCandidate {
        uint64_t address;
        uint64_t instanceId;
        int propertyCount;
        std::map<uint32_t, std::string> stringProperties;
    };
    
    std::vector<EntityCandidate> candidates;
    

    // Read table header
    // NOTE: entitiesTableAddr is the address of the global variable pointing to the table.
    // We must dereference it first.
    
    auto tablePtrBuf = m_memory->readPointer(entitiesTableAddr);
    if (!tablePtrBuf || *tablePtrBuf == 0) {
        spdlog::warn("Entities Table pointer is null at 0x{:X}", entitiesTableAddr);
        return std::nullopt;
    }
    
    uint64_t tableAddr = *tablePtrBuf;
    spdlog::debug("Entities Table at 0x{:X}", tableAddr);

    auto tableHeader = m_memory->readMemory(tableAddr, 8 * 8); // Read enough for header
    if (!tableHeader) {
        spdlog::warn("Could not read Entities Table header");
        return std::nullopt;
    }
    
    // Offsets from EntityTableController.java / DecoderConfig
    // base + 5*ptr = nbBuckets
    // In 64-bit: ptr=8. 
    // Bucket Array Pointer: offset 3*8 = 24
    // First Bucket Pointer: offset 4*8 = 32 ? No, java says getPointer(3L * ptrSize) for bucketsPointer
    // Java: UniversalPointer bucketsPointer = hashTableData.getPointer(3L * (long)pointerSize);
    // nbBuckets = hashTableData.getInt(5L * (long)pointerSize);
    
    uint64_t bucketsArrayPtr = tableHeader->readPointer(3 * 8, true);
    uint32_t nbBuckets = tableHeader->read<uint32_t>(5 * 8);
    uint32_t nbElements = tableHeader->read<uint32_t>(5 * 8 + 4);
    
    spdlog::debug("Entities Table: buckets={}, elements={}, array=0x{:X}", nbBuckets, nbElements, bucketsArrayPtr);
    
    if (bucketsArrayPtr == 0 || nbBuckets == 0 || nbBuckets > 100000) {
        spdlog::warn("Invalid Entities Table data");
        return std::nullopt;
    }
    
    // Read buckets
    // Each bucket entry is a pointer to the first node in the bucket
    auto bucketsBuffer = m_memory->readMemory(bucketsArrayPtr, nbBuckets * 8);
    if (!bucketsBuffer) {
         spdlog::warn("Could not read buckets array");
         return std::nullopt;
    }
    
    int scannedCount = 0;
    
    for (uint32_t i = 0; i < nbBuckets; i++) {
        uint64_t nodePtr = bucketsBuffer->readPointer(i * 8, true);
        
        while (nodePtr != 0) {
            // Read node: [InstanceID (8)] [WorldEntityPtr (8)] [NextPtr (8)] ... maybe
            // EntityTableController:
            // entryBuffer.getLong(0L) -> instanceID
            // entryBuffer.getPointer(WorldEntityOffset) -> worldEntityPointer
            // entryBuffer.getPointer(8L) -> next ?? Wait.
            
            // Check EntityTableController.java line 87: bucketSize = 24 (default? prop says 24).
            // line 94: entity = loadTableEntry(instanceID, worldEntityPointer = entryBuffer.getPointer(WorldEntityOffset))
            // line 98: next = entryBuffer.getPointer(8L)
            
            // If WorldEntityOffset is 16 (from 64bits.properties defaults in code), and Next is at 8.
            // Layout: 0: ID, 8: Next, 16: WorldEntityPtr.
            
            auto nodeBuf = m_memory->readMemory(nodePtr, 32);
            if (!nodeBuf) break;
            
            uint64_t instanceId = nodeBuf->read<uint64_t>(0);
            uint64_t nextPtr = nodeBuf->readPointer(8, true);
            uint64_t worldEntityPtr = nodeBuf->readPointer(16, true); // Assuming Offset 16
            
            if (worldEntityPtr != 0) {
                // Read ConstructionInfo DataID for ALL entities (for equipment lookup)
                // ConstructionInfo pointer at offset 288 (64-bit) / 152 (32-bit)
                auto entityBuf = m_memory->readMemory(worldEntityPtr, 300);
                if (entityBuf) {
                    uint64_t ciOffset = m_config.is64Bit ? 288 : 152;
                    uint64_t ciPtr = entityBuf->readPointer(ciOffset, true);
                    if (ciPtr != 0) {
                        int ptrSize = m_config.is64Bit ? 8 : 4;
                        auto ciBuf = m_memory->readMemory(ciPtr, ptrSize + 8);
                        if (ciBuf) {
                            uint32_t dataId = ciBuf->read<uint32_t>(ptrSize + 4);
                            if (dataId != 0) {
                                m_entityDataIds[instanceId] = dataId;
                            }
                        }
                    }
                    
                    // Also check if this entity has many properties (player candidate)
                    uint64_t eppPtr = entityBuf->readPointer(192, true);
                    if (eppPtr != 0) {
                        auto eppBuf = m_memory->readMemory(eppPtr, 256);
                        if (eppBuf) {
                             uint64_t propBucketsPtr = eppBuf->readPointer(72, true);
                             uint32_t propNbBuckets = eppBuf->read<uint32_t>(88);
                             uint32_t propNbElements = eppBuf->read<uint32_t>(88 + 4);
                             
                             if (propNbElements > 10) {
                                 EntityCandidate candidate;
                                 candidate.address = worldEntityPtr;
                                 candidate.instanceId = instanceId;
                                 candidate.propertyCount = (int)propNbElements;
                                 candidates.push_back(candidate);
                             }
                        }
                    }
                }
            }
            
            nodePtr = nextPtr;
            scannedCount++;
            if (scannedCount > 5000) break; // Safety
        }
    }
    

    spdlog::info("Scanned {} entities, found {} candidates with props > 10, {} entity DataIDs collected", 
                 scannedCount, candidates.size(), m_entityDataIds.size());
    
    // Sort by property count desc
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.propertyCount > b.propertyCount;
    });
    
    // Return the top candidate
    if (!candidates.empty()) {
        auto& top = candidates[0];
        spdlog::info("Found Player Entity candidate: ID {:X}, Props {}, Addr 0x{:X}", 
                     top.instanceId, top.propertyCount, top.address);
        return top.address;
    }
    
    return std::nullopt;

}

// Helper to map Class IDs
static QString mapClassId(int id) {
    // Codes from classes.xml (Agent_Class property values)
    switch (id) {
        case 23: return "Guardian";
        case 24: return "Captain";
        case 31: return "Minstrel";
        case 40: return "Burglar";
        case 162: return "Hunter";
        case 172: return "Champion";
        case 185: return "Lore-master";
        case 193: return "Rune-keeper";
        case 194: return "Warden";
        case 214: return "Beorning";
        case 215: return "Brawler";
        case 216: return "Mariner";
        default: return QString("Unknown (%1)").arg(id);
    }
}

// Helper to map Race IDs (from Agent_Species property)
// Source: LOTRO Companion lotro-data/races/races.xml + Lua plugin main.lua
static QString mapRaceId(int id) {
    switch (id) {
        case 23: return "Man";
        case 65: return "Elf";
        case 73: return "Dwarf";
        case 81: return "Hobbit";
        case 114: return "Beorning";
        case 151: return "High Elf";
        case 152: return "Stout-axe";
        case 153: return "River Hobbit";
        default: return QString("Unknown (%1)").arg(id);
    }
}

std::optional<CharacterInfo> CharacterExtractor::extractCharacter() {
    if (!isConnected()) {
        m_lastError = "Not connected to LOTRO client";
        return std::nullopt;
    }
    
    CharacterInfo info;
    
    // Get server name
    auto server = getServerName();
    if (server) {
        info.server = *server;
    }
    
    // Get account info
    auto account = getAccountName();
    if (account) {
        info.account = *account;
    }
    
    // Read account type
    if (m_accountTypePropertyId != -1) {
        auto accType = readAccountIntProperty(m_accountTypePropertyId);
        if (accType) {
             int typeCode = *accType;
             switch(typeCode) {
                 case 1: info.accountType = CharacterInfo::AccountType::FreeToPlay; break; // "Free To Play"
                 case 3: info.accountType = CharacterInfo::AccountType::Premium; break;
                 case 6: info.accountType = CharacterInfo::AccountType::VIP; break;
                 case 7: info.accountType = CharacterInfo::AccountType::Lifetime; break;
                 default: info.accountType = CharacterInfo::AccountType::Unknown; break;
             }
        }
    }
    
    // Account Level Currencies (Destiny Points, LOTRO Points)
    if (m_destinyPointsPropertyId != -1) {
        auto val = readAccountIntProperty(m_destinyPointsPropertyId);
        if (val) {
            info.destinyPoints = *val;
            spdlog::info("Destiny Points: {}", *val);
        } else {
            spdlog::warn("Failed to read destiny points (propId={})", m_destinyPointsPropertyId);
        }
    } else {
        spdlog::warn("Destiny points property ID not resolved from DAT");
    }

    
    // Find player entity
    auto playerEntity = findPlayerEntity();
    if (!playerEntity) {
        m_lastError = "Player entity not found";
        return std::nullopt;
    }
    
    // One-shot property discovery: scan ALL properties on the entity
    // to find the actual property IDs for class, race, money, vitals
    static bool propertyDiscoveryDone = false;
    if (!propertyDiscoveryDone && m_datFacade) {
        propertyDiscoveryDone = true;
        spdlog::info("=== ENTITY PROPERTY DISCOVERY (Entity 0x{:X}) ===", *playerEntity);
        
        // Get EPP
        uint64_t eppOffset = m_config.is64Bit ? 192 : 108;
        auto entBuf = m_memory->readMemory(*playerEntity, 256);
        if (entBuf) {
            uint64_t eppPtr = entBuf->readPointer(eppOffset, true);
            if (eppPtr != 0) {
                auto eppBuf = m_memory->readMemory(eppPtr, 128);
                if (eppBuf) {
                    uint64_t bucketsPtr = eppBuf->readPointer(72, true);
                    uint32_t nbBuckets = eppBuf->read<uint32_t>(88);
                    
                    spdlog::info("EPP hashtable: bucketsPtr=0x{:X}, nbBuckets={}", bucketsPtr, nbBuckets);
                    
                    if (bucketsPtr != 0 && nbBuckets > 0 && nbBuckets < 10000) {
                        auto bucketsBuf = m_memory->readMemory(bucketsPtr, nbBuckets * 8);
                        if (bucketsBuf) {
                            int totalProps = 0;
                            int matchedProps = 0;
                            auto* registry = m_datFacade->getPropertiesRegistry();
                            
                            // Keywords to filter for 
                            std::vector<std::string> keywords = {
                                "class", "race", "species", "money", "currency", "gold", "silver", "copper",
                                "vital", "morale", "power", "health", "mana",
                                "level", "wallet", "inventory", "lotro", "mithril",
                                "advancement", "advtable", "agent", "title", "rank", "surname"
                            };
                            
                            for (uint32_t i = 0; i < nbBuckets; i++) {
                                uint64_t nodePtr = bucketsBuf->readPointer(i * 8, true);
                                
                                while (nodePtr != 0) {
                                    auto nodeBuf = m_memory->readMemory(nodePtr, 32);
                                    if (!nodeBuf) break;
                                    
                                    uint32_t id = nodeBuf->read<uint32_t>(0);
                                    uint64_t value = nodeBuf->read<uint64_t>(24);
                                    uint64_t nextPtr = nodeBuf->readPointer(8, true);
                                    totalProps++;
                                    
                                    // Resolve name from DAT registry
                                    std::string propName = "UNKNOWN";
                                    dat::PropertyType propType = dat::PropertyType::UNKNOWN;
                                    if (registry) {
                                        auto def = registry->getPropertyDef(id);
                                        if (def) {
                                            propName = def->name().toStdString();
                                            propType = def->type();
                                        }
                                    }
                                    
                                    // Check if name matches any keyword
                                    std::string lowerName = propName;
                                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                                    
                                    bool matches = false;
                                    for (const auto& kw : keywords) {
                                        if (lowerName.find(kw) != std::string::npos) {
                                            matches = true;
                                            break;
                                        }
                                    }
                                    
                                    if (matches) {
                                        spdlog::info("  MATCH: {} (ID={}, type={}, value=0x{:X}, int32={})",
                                                     propName, id, static_cast<int>(propType), value, static_cast<int32_t>(value));
                                        matchedProps++;
                                    }
                                    
                                    nodePtr = nextPtr;
                                }
                            }
                            spdlog::info("=== Discovery complete: {} total properties, {} keyword matches ===", 
                                         totalProps, matchedProps);
                        }
                    }
                }
            }
        }
    }
    
    // Read basic info via properties
    // Read Name
    if (m_namePropertyId != -1) {
        auto name = readStringProperty(*playerEntity, m_namePropertyId);
        if (name) {
            info.name = *name;
        } else {
            // Brute force scan fallback if property lookup fails
            // (Assuming existing code had fallback logic but it was likely inside readStringProperty)
            // If name is empty here, extraction failed partially.
        }
    }
    
    // Read Level
    if (m_levelPropertyId != -1) {
        auto level = readIntProperty(*playerEntity, m_levelPropertyId);
        if (level) {
            info.level = *level;
        }
    }
    
    // Read Class
    if (m_classPropertyId != -1) {
        // Try as raw value first for diagnostics
        auto rawVal = readPropertyValue(*playerEntity, m_classPropertyId);
        if (rawVal) {
            spdlog::info("Class raw property value: 0x{:X} (int32={})", *rawVal, static_cast<int>(*rawVal));
            int classId = static_cast<int>(*rawVal);
            info.className = mapClassId(classId);
            spdlog::info("Class ID {} -> {}", classId, info.className.toStdString());
        } else {
            spdlog::warn("Failed to read class property {} from entity 0x{:X}", m_classPropertyId, *playerEntity);
        }
    } else {
        spdlog::warn("Class property ID not resolved from DAT (name='Agent_Class')");
    }
    
    // Read Race via Agent_Species property (confirmed working from entity property discovery)
    // Known race codes: Man=23, Elf=65, Dwarf=73, Hobbit=81, Beorning=114, High Elf=151, Stout-axe=152, River Hobbit=153
    if (m_racePropertyId != -1) {
        auto rawVal = readPropertyValue(*playerEntity, m_racePropertyId);
        if (rawVal) {
            spdlog::info("Race raw property value: 0x{:X} (int32={})", *rawVal, static_cast<int>(*rawVal));
            int raceId = static_cast<int>(*rawVal);
            info.race = mapRaceId(raceId);
            spdlog::info("Race ID {} -> {}", raceId, info.race.toStdString());
        } else {
            spdlog::warn("Failed to read race property {} from entity 0x{:X}", m_racePropertyId, *playerEntity);
        }
    } else {
        spdlog::warn("Race property ID not resolved from DAT");
    }
    
    // Read Vitals (Morale/Power)
    // LOTRO stores vitals as FLOAT properties, not INT
    if (m_currentMoralePropertyId != -1) {
        auto val = readFloatProperty(*playerEntity, m_currentMoralePropertyId);
        if (val) {
            info.morale = static_cast<int>(*val);
            spdlog::debug("Current Morale (float): {} -> {}", *val, info.morale);
        } else {
            spdlog::debug("Failed to read current morale property {}", m_currentMoralePropertyId);
        }
    }
    if (m_currentPowerPropertyId != -1) {
        auto val = readFloatProperty(*playerEntity, m_currentPowerPropertyId);
        if (val) {
            info.power = static_cast<int>(*val);
            spdlog::debug("Current Power (float): {} -> {}", *val, info.power);
        } else {
            spdlog::debug("Failed to read current power property {}", m_currentPowerPropertyId);
        }
    }
    
    // Read Money (Copper) - stored as Long (INT64)
    // If Long fails, try Int (some versions store as 32-bit)
    if (m_moneyPropertyId != -1) {
        auto rawVal = readPropertyValue(*playerEntity, m_moneyPropertyId);
        if (rawVal) {
            spdlog::info("Money raw property value: 0x{:X}", *rawVal);
            // Try interpreting as signed 64-bit first
            int64_t copperTotal = static_cast<int64_t>(*rawVal);
            if (copperTotal > 0 && copperTotal < 100000000000LL) {
                // LOTRO: 1 gold = 1000 silver, 1 silver = 100 copper  
                // So 1 gold = 100,000 copper
                info.gold = static_cast<int>(copperTotal / 100000);
                info.silver = static_cast<int>((copperTotal / 100) % 1000);
                info.copper = static_cast<int>(copperTotal % 100);
                spdlog::info("Money: {} copper = {}g {}s {}c", copperTotal, info.gold, info.silver, info.copper);
            } else {
                // Try as 32-bit int
                int copperInt = static_cast<int>(*rawVal);
                if (copperInt > 0) {
                    info.gold = copperInt / 100000;
                    info.silver = (copperInt / 100) % 1000;
                    info.copper = copperInt % 100;
                    spdlog::info("Money (32-bit): {} copper = {}g {}s {}c", copperInt, info.gold, info.silver, info.copper);
                } else {
                    spdlog::warn("Money value doesn't look valid: raw=0x{:X}, int64={}, int32={}",
                                 *rawVal, copperTotal, copperInt);
                }
            }
        } else {
            spdlog::warn("Failed to read money property {} from entity", m_moneyPropertyId);
        }
    } else {
        spdlog::warn("Money property ID not resolved from DAT (name='Currency_Amount')");
    }

    
    spdlog::info("Extracted: {} Lv{} {} {}, Morale {}, Power {}", 
                 info.name.toStdString(), info.level, info.race.toStdString(), info.className.toStdString(),
                 info.morale, info.power);
    
    return info;
}

std::optional<CharacterData> CharacterExtractor::extractFullData() {
    // First get basic character info
    auto basicInfo = extractCharacter();
    if (!basicInfo) {
        return std::nullopt;
    }
    
    CharacterData data;
    data.basic = *basicInfo;
    
    // Find player entity again for extended properties
    auto playerEntity = findPlayerEntity();
    if (!playerEntity) {
        return data;  // Return basic info only
    }
    
    auto* registry = m_datFacade ? m_datFacade->getPropertiesRegistry() : nullptr;
    if (!registry) {
        spdlog::warn("No property registry available for extended extraction");
        return data;
    }
    
    // === Extract Virtues ===
    // Property names like "Trait_Virtue_Rank_Charity", "Trait_Virtue_XP_Charity"
    const std::vector<std::pair<QString, QString>> virtueKeys = {
        {"CHARITY",      "Charity"},
        {"COMPASSION",   "Compassion"},
        {"CONFIDENCE",   "Confidence"},
        {"DETERMINATION","Determination"},
        {"DISCIPLINE",   "Discipline"},
        {"EMPATHY",      "Empathy"},
        {"FIDELITY",     "Fidelity"},
        {"FORTITUDE",    "Fortitude"},
        {"HONESTY",      "Honesty"},
        {"HONOUR",       "Honour"},
        {"IDEALISM",     "Idealism"},
        {"INNOCENCE",    "Innocence"},
        {"JUSTICE",      "Justice"},
        {"LOYALTY",      "Loyalty"},
        {"MERCY",        "Mercy"},
        {"PATIENCE",     "Patience"},
        {"TOLERANCE",    "Tolerance"},
        {"VALOUR",       "Valour"},
        {"WISDOM",       "Wisdom"},
        {"WIT",          "Wit"},
        {"ZEAL",         "Zeal"}
    };
    
    // Virtue property name suffixes (from XML analysis)
    const std::map<QString, QString> virtueRankSuffix = {
        {"CHARITY", "Charity"}, {"COMPASSION", "Compassionate"}, {"CONFIDENCE", "Confidence"},
        {"DETERMINATION", "Determination"}, {"DISCIPLINE", "Discipline"}, {"EMPATHY", "Empathy"},
        {"FIDELITY", "Fidelity"}, {"FORTITUDE", "Fortitude"}, {"HONESTY", "Honesty"},
        {"HONOUR", "Honour"}, {"IDEALISM", "Idealism"}, {"INNOCENCE", "Innocence"},
        {"JUSTICE", "Just"}, {"LOYALTY", "Loyalty"}, {"MERCY", "Merciful"},
        {"PATIENCE", "Patience"}, {"TOLERANCE", "Tolerant"}, {"VALOUR", "Valor"},
        {"WISDOM", "Wisdom"}, {"WIT", "Wit"}, {"ZEAL", "Zeal"}
    };
    
    for (const auto& [key, displayName] : virtueKeys) {
        VirtueStatus vs;
        vs.key = key;
        vs.name = displayName;
        
        // Get property IDs
        QString suffixIt = virtueRankSuffix.count(key) ? virtueRankSuffix.at(key) : displayName;
        QString rankProp = QString("Trait_Virtue_Rank_%1").arg(suffixIt);
        QString xpProp = QString("Trait_Virtue_XP_%1").arg(suffixIt);
        
        int rankPropId = registry->getPropertyId(rankProp.toStdString().c_str());
        int xpPropId = registry->getPropertyId(xpProp.toStdString().c_str());
        
        if (rankPropId != -1) {
            if (auto rank = readIntProperty(*playerEntity, rankPropId)) {
                vs.rank = *rank;
            }
        }
        
        if (xpPropId != -1) {
            if (auto xp = readIntProperty(*playerEntity, xpPropId)) {
                vs.xp = *xp;
            }
        }
        
        data.virtues.push_back(vs);
    }
    
    spdlog::info("Extracted {} virtues", data.virtues.size());
    
    // === Extract Reputation ===
    // Property names like "Reputation_Faction_Breeland_Men_CurrentTier"
    // Faction keys: {key, display_name, property_prefix, category}
    // Property prefixes must exactly match factions.xml currentTierProperty/currentReputationProperty
    struct FactionDef { QString key; QString name; QString propPrefix; QString category; };
    const std::vector<FactionDef> factionKeys = {
        // Eriador
        {"BREE", "Men of Bree", "Reputation_Faction_Breeland_Men", "Eriador"},
        {"SHIRE", "The Mathom Society", "Reputation_Faction_Shire_Mathoms", "Eriador"},
        {"DWARVES", "Thorin's Hall", "Reputation_Faction_Eredluin_Dwarves", "Eriador"},
        {"EGLAIN", "The Eglain", "Reputation_Faction_Lonelands_Eglain", "Eriador"},
        {"ESTELDIN", "Rangers of Esteldín", "Reputation_Faction_Northdowns_Esteldin", "Eriador"},
        {"RIVENDELL", "Elves of Rivendell", "Reputation_Faction_Rivendell_Elves", "Eriador"},
        {"ANNUMINAS", "The Wardens of Annúminas", "Reputation_Faction_Evendim_Rangers", "Eriador"},
        {"COUNCIL_OF_THE_NORTH", "Council of the North", "Reputation_Faction_Angmar_Free_People", "Eriador"},
        {"LOSSOTH", "Lossoth of Forochel", "Reputation_Faction_Forochel_Lossoth", "Eriador"},
        // Rhovanion
        {"MORIA_GUARDS", "Iron Garrison Guards", "Reputation_Faction_Moria_Dwarves_Fast", "Rhovanion"},
        {"MORIA_MINERS", "Iron Garrison Miners", "Reputation_Faction_Moria_Dwarves_Slow", "Rhovanion"},
        {"GALADHRIM", "Galadhrim", "Reputation_Faction_Lorien_Elves", "Rhovanion"},
        {"MALLEDHRIM", "Malledhrim", "Reputation_Faction_Mirkwood_Offensive", "Rhovanion"},
        {"ELVES_OF_FELEGOTH", "Elves of Felegoth", "Reputation_Faction_Mirkwood_North_Elves", "Rhovanion"},
        {"MEN_OF_DALE", "Men of Dale", "Reputation_Faction_Mirkwood_North_Men", "Rhovanion"},
        {"DWARVES_OF_EREBOR", "Dwarves of Erebor", "Reputation_Faction_Mirkwood_North_Dwarves", "Rhovanion"},
        {"GREY_MOUNTAINS_EXPEDITION", "Grey Mountains Expedition", "Reputation_Faction_Dwarfholds_Eredmithrin", "Rhovanion"},
        {"WILDERFOLK", "Wilderfolk", "Reputation_Faction_Vales_Of_Anduin", "Rhovanion"},
        // Dunland
        {"ALGRAIG", "Algraig, Men of Enedwaith", "Reputation_Faction_Enedwaith_Dunlendings", "Dunland"},
        {"GREY_COMPANY", "The Grey Company", "Reputation_Faction_Enedwaith_Grey_Company", "Dunland"},
        {"DUNLAND", "Men of Dunland", "Reputation_Faction_Dunland_Dunlendings", "Dunland"},
        {"THEODRED_RIDERS", "Théodred's Riders", "Reputation_Faction_Dunland_Theodred", "Dunland"},
        // Rohan
        {"STANGARD_RIDERS", "The Riders of Stangard", "Reputation_Faction_Greatriver_Stangard", "Rohan"},
        {"WOLD", "Men of the Wold", "Reputation_Faction_Rohan_Wold", "Rohan"},
        {"NORCROFTS", "Men of the Norcrofts", "Reputation_Faction_Rohan_Norcrofts", "Rohan"},
        {"ENTWASH_VALE", "Men of the Entwash Vale", "Reputation_Faction_Rohan_Entwashvale", "Rohan"},
        {"SUTCROFTS", "Men of the Sutcrofts", "Reputation_Faction_Rohan_Sutcrofts", "Rohan"},
        {"EORLINGAS", "The Eorlingas", "Reputation_Faction_Rohan_West_Eorlingas", "Rohan"},
        {"HELMINGAS", "The Helmingas", "Reputation_Faction_Rohan_West_Helmingas", "Rohan"},
        {"FANGORN", "The Ents of Fangorn Forest", "Reputation_Faction_Rohan_West_Fangorn", "Rohan"},
        {"PEOPLE_WILDERMORE", "People of Wildermore", "Reputation_Faction_Wildermore_Basic", "Rohan"},
        // Gondor
        {"DOL_AMROTH", "Dol Amroth", "Reputation_Faction_Gondor_West_Amroth", "Gondor"},
        {"PELARGIR", "Pelargir", "Reputation_Faction_Gondor_Central_Pelargir", "Gondor"},
        {"RANGERS_ITHILIEN", "Rangers of Ithilien", "Reputation_Faction_Gondor_East_Rangers", "Gondor"},
        {"MINAS_TIRITH", "Defenders of Minas Tirith", "Reputation_Faction_Gondor_Minas_Tirith", "Gondor"},
        // Mordor
        {"HOST_OF_THE_WEST", "Host of the West", "Reputation_Faction_Mountains_Shadow_Host_West", "Mordor"},
        {"GORGOROTH", "Conquest of Gorgoroth", "Reputation_Faction_Mordor_Gorgoroth", "Mordor"},
    };
    
    for (const auto& fd : factionKeys) {
        FactionStatus fs;
        fs.key = fd.key;
        fs.name = fd.name;
        fs.category = fd.category;
        
        QString tierProp = QString("%1_CurrentTier").arg(fd.propPrefix);
        QString repProp = QString("%1_EarnedReputation").arg(fd.propPrefix);
        
        int tierPropId = registry->getPropertyId(tierProp.toStdString().c_str());
        int repPropId = registry->getPropertyId(repProp.toStdString().c_str());
        
        if (tierPropId != -1) {
            if (auto tier = readIntProperty(*playerEntity, tierPropId)) {
                fs.tier = *tier;
            }
        }
        
        if (repPropId != -1) {
            if (auto rep = readIntProperty(*playerEntity, repPropId)) {
                fs.reputation = *rep;
            }
        }
        
        if (fs.tier > 0 || fs.reputation > 0) {
            data.factions.push_back(fs);
        }
    }
    
    spdlog::info("Extracted {} faction reputations", data.factions.size());
    
    // === Extract Crafting Professions ===
    // Property names like "Craft_Scholar_MasteryLevel", "Craft_Scholar_ProficiencyLevel"
    const std::vector<std::pair<QString, QString>> professionKeys = {
        {"SCHOLAR",     "Scholar"},
        {"METALSMITH",  "Metalsmith"},
        {"JEWELLER",    "Jeweller"},
        {"TAILOR",      "Tailor"},
        {"WEAPONSMITH", "Weaponsmith"},
        {"WOODWORKER",  "Woodworker"},
        {"COOK",        "Cook"},
        {"FARMER",      "Farmer"},
        {"FORESTER",    "Forester"},
        {"PROSPECTOR",  "Prospector"}
    };
    
    for (const auto& [key, name] : professionKeys) {
        QString enabledProp = QString("Craft_%1_Enabled").arg(name);
        int enabledPropId = registry->getPropertyId(enabledProp.toStdString().c_str());
        
        bool isEnabled = false;
        if (enabledPropId != -1) {
            if (auto val = readIntProperty(*playerEntity, enabledPropId)) {
                isEnabled = *val != 0;
            }
        }
        
        if (isEnabled) {
            CraftingProfessionStatus cps;
            cps.name = name;
            
            QString masteryProp = QString("Craft_%1_MasteryLevel").arg(name);
            QString profProp = QString("Craft_%1_ProficiencyLevel").arg(name);
            QString masteryXpProp = QString("Craft_%1_MasteryXP").arg(name);
            QString profXpProp = QString("Craft_%1_ProficiencyXP").arg(name);
            
            int masteryPropId = registry->getPropertyId(masteryProp.toStdString().c_str());
            int profPropId = registry->getPropertyId(profProp.toStdString().c_str());
            int masteryXpPropId = registry->getPropertyId(masteryXpProp.toStdString().c_str());
            int profXpPropId = registry->getPropertyId(profXpProp.toStdString().c_str());
            
            if (masteryPropId != -1) {
                if (auto val = readIntProperty(*playerEntity, masteryPropId)) {
                    cps.mastery = *val;
                }
            }
            if (profPropId != -1) {
                if (auto val = readIntProperty(*playerEntity, profPropId)) {
                    cps.tier = *val;
                }
            }
            if (masteryXpPropId != -1) {
                if (auto val = readIntProperty(*playerEntity, masteryXpPropId)) {
                    cps.hasMastered = (*val > 0);
                }
            }
            if (profXpPropId != -1) {
                if (auto val = readIntProperty(*playerEntity, profXpPropId)) {
                    cps.proficiency = *val;
                }
            }
            
            data.crafting.professions.push_back(cps);
        }
    }
    
    spdlog::info("Extracted {} crafting professions", data.crafting.professions.size());
    
    // === Extract Equipment ===
    // Equipment slots use Inventory_SlotCache_Eq_* property names (type 7 = entity ID).
    // The value is an entity ID pointing to the item entity in the entities table.
    // To get the actual item DID, we follow the entity pointer and read its
    // ConstructionInfo.DataID (offset 288+8=296 from entity base, 64-bit).
    const std::vector<std::pair<QString, QString>> equipmentSlots = {
        {"HEAD",       "Inventory_SlotCache_Eq_Head"},
        {"SHOULDERS",  "Inventory_SlotCache_Eq_Shoulder"},
        {"CHEST",      "Inventory_SlotCache_Eq_Chest"},
        {"HANDS",      "Inventory_SlotCache_Eq_Gloves"},
        {"LEGS",       "Inventory_SlotCache_Eq_Legs"},
        {"FEET",       "Inventory_SlotCache_Eq_Boots"},
        {"BACK",       "Inventory_SlotCache_Eq_Back"},
        {"MAIN_HAND",  "Inventory_SlotCache_Eq_Weapon_Primary"},
        {"OFF_HAND",   "Inventory_SlotCache_Eq_Weapon_Secondary"},
        {"RANGED",     "Inventory_SlotCache_Eq_RangedWeapon"},
        {"POCKET",     "Inventory_SlotCache_Eq_Pocket1"},
        {"EAR1",       "Inventory_SlotCache_Eq_Earring1"},
        {"EAR2",       "Inventory_SlotCache_Eq_Earring2"},
        {"NECK",       "Inventory_SlotCache_Eq_Necklace"},
        {"WRIST1",     "Inventory_SlotCache_Eq_Bracelet1"},
        {"WRIST2",     "Inventory_SlotCache_Eq_Bracelet2"},
        {"RING1",      "Inventory_SlotCache_Eq_Ring1"},
        {"RING2",      "Inventory_SlotCache_Eq_Ring2"},
        {"CLASS_SLOT", "Inventory_SlotCache_Eq_Class"},
        {"CRAFT_TOOL", "Inventory_SlotCache_Eq_CraftTool"},
    };
    
    for (const auto& [slotName, propName] : equipmentSlots) {
        int propId = registry->getPropertyId(propName);
        if (propId == -1) {
            spdlog::debug("Equipment property '{}' not found in registry", propName.toStdString());
            continue;
        }
        
        // Slot cache properties are INSTANCE_ID type (type 7).
        // The value at offset 24 in the hashtable entry is a POINTER to a
        // ref-counted Long64 struct:
        //   +0: vtable/refcount header (refCountTemplateSize = ptrSize + intSize = 16 for 64-bit)
        //   +16: 64-bit entity instance ID
        // We then look up this instance ID in the entities table DataID map.
        
        auto rawVal = readPropertyValue(*playerEntity, propId);
        if (!rawVal || *rawVal == 0) continue;
        
        uint64_t instanceIdPtr = *rawVal;
        
        // Read the entity instance ID from the ref-counted Long64 struct
        // refCountTemplateSize = pointerSize + intSize (from Java Constants.CONFIG)
        // For 64-bit: 8 + 8 = 16
        int refCountSize = m_config.is64Bit ? 16 : 8;
        auto instanceBuf = m_memory->readMemory(instanceIdPtr, refCountSize + 8);
        if (!instanceBuf) continue;
        
        uint64_t entityInstanceId = instanceBuf->read<uint64_t>(refCountSize);
        if (entityInstanceId == 0) continue;
        
        // Look up the entity instance ID in our DataID map (populated during entity scan)
        auto it = m_entityDataIds.find(entityInstanceId);
        if (it != m_entityDataIds.end()) {
            uint32_t dataId = it->second;
            data.equippedGear[slotName] = static_cast<int>(dataId);
            spdlog::info("Slot {}: instanceId 0x{:X} -> item DID 0x{:X} ({})", 
                         slotName.toStdString(), entityInstanceId, dataId, dataId);
        } else {
            spdlog::debug("Slot {}: instanceId 0x{:X} not found in entity DataID map ({} entries)", 
                         slotName.toStdString(), entityInstanceId, m_entityDataIds.size());
        }
    }
    
    spdlog::info("Extracted {} equipped items", data.equippedGear.size());
    
    // === Extract Wallet Currencies ===
    // Common wallet currencies with their property names
    const std::vector<std::pair<int, QString>> currencyProperties = {
        {1, "Wallet_Currency_Marks"},
        {2, "Wallet_Currency_Medallions"},
        {3, "Wallet_Currency_Seals"},
        {4, "Wallet_Currency_MithrilCoins"},
        {5, "Wallet_Currency_Commendations"},
        {6, "Wallet_Currency_Skirmish_Marks"},
        {7, "Wallet_Currency_Fate_Tokens"},
        {8, "Wallet_Currency_Hobbit_Presents"},
        {9, "Wallet_Currency_Silver_Tokens"},
        {10, "Wallet_Currency_Gift_Mathom"},
        {11, "Wallet_Currency_Crafting_Guild"},
        {12, "Wallet_Currency_AnniversaryTokens"},
        {13, "Wallet_Currency_FestivalTokens"},
        {14, "Wallet_Currency_Figments"},
        {15, "Wallet_Currency_MoriaShards"},
    };
    
    for (const auto& [currencyId, propName] : currencyProperties) {
        int propId = registry->getPropertyId(propName.toStdString().c_str());
        if (propId != -1) {
            if (auto val = readIntProperty(*playerEntity, propId)) {
                if (*val > 0) {
                    data.wallet[currencyId] = *val;
                }
            }
        }
    }
    
    spdlog::info("Extracted {} wallet currencies", data.wallet.size());
    
    // === Extract Current Title ===
    // Title_ActiveTitleDID contains the DID reference to the active title (type 20 = DATA_FILE)
    // Title_ActiveTitleString contains the display string (type 13 = STRING_INFO)
    // Discovered via entity property dump: value 0x70020442 = 1879180354 = "Eglan-friend"
    {
        static const std::vector<std::string> titleCandidates = {
            "Title_ActiveTitleDID",       // Confirmed working - contains title DID
            "Advancement_CurrentTitle", "AdvTable_CurrentTitle", "Player_CurrentTitle",
            "Agent_CurrentTitle", "CurrentTitle"
        };
        
        int titlePropId = -1;
        std::string resolvedName;
        for (const auto& name : titleCandidates) {
            int id = registry->getPropertyId(QString::fromStdString(name));
            if (id != -1) {
                titlePropId = id;
                resolvedName = name;
                spdlog::info("Title property '{}' resolved to ID {}", name, id);
                break;
            }
        }
        
        if (titlePropId != -1) {
            // Read as raw property value (DATA_FILE type stores a DID reference)
            auto rawVal = readPropertyValue(*playerEntity, titlePropId);
            if (rawVal) {
                int titleId = static_cast<int>(static_cast<uint32_t>(*rawVal));
                if (titleId > 0) {
                    data.titles.push_back(titleId);
                    spdlog::info("Active title DID: {} (0x{:X}) from entity prop '{}'", titleId, titleId, resolvedName);
                }
            }
            // Try storage if entity failed
            if (data.titles.empty()) {
                auto rawVal = readStoragePropertyValue(titlePropId);
                if (rawVal && *rawVal > 0) {
                    data.titles.push_back(static_cast<int>(static_cast<uint32_t>(*rawVal)));
                    spdlog::info("Active title DID: {} (from storage, prop='{}')", *rawVal, resolvedName);
                }
            }
        } else {
            spdlog::warn("No title property name resolved from DAT. Tried: {}", 
                        [&]() { std::string s; for (const auto& n : titleCandidates) { if (!s.empty()) s += ", "; s += n; } return s; }());
        }
    }
    
    // NOTE: Active title extracted above from entity properties.
    // Now try to get ALL acquired titles from WSL References Table.
    {
        auto wslTitles = extractTitlesFromWSL();
        if (!wslTitles.empty()) {
            // Merge WSL titles with any active title already found
            std::set<int> titleSet(data.titles.begin(), data.titles.end());
            for (int tid : wslTitles) {
                titleSet.insert(tid);
            }
            data.titles.assign(titleSet.begin(), titleSet.end());
            spdlog::info("Total titles after WSL merge: {} (active + {} from WSL)", 
                         data.titles.size(), wslTitles.size());
        } else {
            spdlog::info("Active title only: DID {} (0x{:X})", 
                         data.titles.empty() ? 0 : data.titles[0],
                         data.titles.empty() ? 0u : static_cast<uint32_t>(data.titles[0]));
        }
    }
    
    // === Extract Emotes ===
    // Emote_GrantedList is an ARRAY property containing emote IDs
    {
        int emotePropId = registry->getPropertyId("Emote_GrantedList");
        if (emotePropId != -1) {
            auto emoteIds = readArrayProperty(*playerEntity, emotePropId);
            for (int emoteId : emoteIds) {
                if (emoteId > 0) {
                    data.emotes.push_back(emoteId);
                }
            }
            spdlog::info("Extracted {} emotes from Emote_GrantedList", data.emotes.size());
        } else {
            spdlog::debug("Emote_GrantedList property not found in registry");
        }
    }
    
    return data;
}

std::optional<uint64_t> CharacterExtractor::readPropertyValue(uint64_t entityAddress, uint32_t propId) {
    if (!isConnected()) return std::nullopt;

    // Get EntityPropertyProvider pointer (offset 192 in 64-bit)
    // TODO: Handle 32-bit offset (108)
    uint64_t eppOffset = m_config.is64Bit ? 192 : 108;
    
    auto entBuf = m_memory->readMemory(entityAddress, 256);
    if (!entBuf) return std::nullopt;
    
    uint64_t eppPtr = entBuf->readPointer(eppOffset, true);
    if (eppPtr == 0) return std::nullopt;
    
    // Hashtable base for entities is at eppPtr + 56 (for 64-bit)
    uint64_t hashtableBase = eppPtr + (m_config.is64Bit ? 56 : 32); 
    
    return readHashtableValue(hashtableBase, propId);
}

std::optional<QString> CharacterExtractor::readStringProperty(uint64_t entityAddress, uint32_t propId) {
    auto val = readPropertyValue(entityAddress, propId);
    if (!val) return std::nullopt;
    
    uint64_t valPtr = *val;
    
    // Check if valid pointer
    if (valPtr < 0x10000 || valPtr > 0x7FFFFFFFFFFF) {
        return std::nullopt;
    }
    
    // For STRING type properties:
    // The value pointer points to a StringPropValue object.
    // Layout (64-bit based on decompiled LOTRO Companion 6.4):
    //   intSize = 8 (not 4!)
    //   refCountTemplateSize = pointerSize + intSize = 8 + 8 = 16
    // So string pointer is at offset 16 (not 12).
    
    // Verify property type
    dat::PropertyType propType = dat::PropertyType::STRING;
    if (m_datFacade) {
        if (auto* registry = m_datFacade->getPropertiesRegistry()) {
             if (auto def = registry->getPropertyDef(propId)) {
                 propType = def->type();
             }
        }
    }
    
    // Read object buffer (enough for STRING_INFO)
    auto objBuf = m_memory->readMemory(valPtr, 96);
    if (!objBuf) return std::nullopt;
    // Case 1: STRING_INFO (Type 13)
    // Based on decompiled LOTRO Companion 6.4 with intSize=8 for 64-bit:
    // - refCountTemplateSize = pointerSize(8) + intSize(8) = 16
    // - hashTableSize = 4 * pointerSize + 4 + 4 = 40
    // - stringPointerOffset = 16 + 8 + 4 + 4 + 40 = 72
    // - isLiteralOffset = 72 + pointerSize = 72 + 8 = 80
    // - tokenIDOffset = refCountTemplateSize + pointerSize = 16 + 8 = 24
    // - tableIDOffset = tokenIDOffset + 4 = 28
    if (propType == dat::PropertyType::STRING_INFO) {
        // First try: String pointer at offset 72 (UTF-16 null-terminated)
        // This is where character names are stored
        uint64_t strPtr72 = objBuf->readPointer(72, true);
        if (strPtr72 > 0x10000 && strPtr72 < 0x7FFFFFFFFFFF) {
            // Try UTF-16 first (this is what worked in testing)
            auto ws = m_memory->readWideString(strPtr72, 128);
            if (ws && !ws->empty()) {
                return QString::fromStdWString(*ws);
            }
            // Fallback to Latin-1
            auto s = m_memory->readString(strPtr72, 128);
            if (s && !s->empty()) {
                return QString::fromLatin1(s->c_str());
            }
        }
        
        // Second try: Check if isLiteral flag is set (offset 80)
        uint32_t isLiteralInt = objBuf->read<uint32_t>(80);
        bool isLiteral = (isLiteralInt & 1) != 0;
        
        if (isLiteral) {
            // Literal string pointer at offset 72
            uint64_t strPtr = objBuf->readPointer(72, true);
            if (strPtr > 0x10000 && strPtr < 0x7FFFFFFFFFFF) {
                // Try UTF-16 with length at (strPtr - 4)
                auto lenBuf = m_memory->readMemory(strPtr - 4, 4);
                if (lenBuf) {
                    uint32_t len = lenBuf->read<uint32_t>(0);
                    if (len > 0 && len < 1000) {
                        auto ws = m_memory->readWideString(strPtr, (len - 1) * 2);
                        if (ws && !ws->empty()) {
                            return QString::fromStdWString(*ws);
                        }
                    }
                }
                // Fallback: null-terminated UTF-16
                auto ws = m_memory->readWideString(strPtr, 128);
                if (ws && !ws->empty()) {
                    return QString::fromStdWString(*ws);
                }
            }
        } else {
            // Table lookup (Token at offset 24, Table at offset 28)
            uint32_t tokenID = objBuf->read<uint32_t>(24);
            uint32_t tableID = objBuf->read<uint32_t>(28);
            
            if (m_datFacade && tableID != 0) {
                QString resolved = m_datFacade->resolveString(tableID, tokenID);
                if (!resolved.isEmpty()) {
                    return resolved;
                }
            }
        }
        
        // Final fallback: scan other pointer offsets
        for (int off : {0, 16, 32, 40}) {
            uint64_t ptr = objBuf->readPointer(off, true);
            if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                auto s = m_memory->readString(ptr, 64);
                if (s && s->length() > 1 && s->length() < 50) {
                    bool printable = true;
                    for (char c : *s) {
                        if ((unsigned char)c < 32 || (unsigned char)c > 126) {
                            printable = false;
                            break;
                        }
                    }
                    if (printable) {
                        return QString::fromLatin1(s->c_str());
                    }
                }
            }
        }
    }
    
    // Case 2: Standard STRING (Type 1 or 4) or Fallback
    // For STRING type: string pointer at refCountTemplateSize(16) - decompiled source uses this
    
    // Try Offset 16 (correct for 64-bit with intSize=8)
    uint64_t strPtr16 = objBuf->readPointer(16, true);
    if (strPtr16 > 0x10000 && strPtr16 < 0x7FFFFFFFFFFF) {
        auto s = m_memory->readString(strPtr16, 128);
        if (s && !s->empty()) return QString::fromLatin1(s->c_str());
        
        auto ws = m_memory->readWideString(strPtr16, 128);
        if (ws && !ws->empty()) return QString::fromStdWString(*ws);
    }

    return std::nullopt;
}

std::optional<int> CharacterExtractor::readIntProperty(uint64_t entityAddress, uint32_t propId) {
    auto val = readPropertyValue(entityAddress, propId);
    if (!val) return std::nullopt;
    
    // Int is 32-bit, but we read 64-bit value container.
    // Just cast to int.
    return static_cast<int>(*val);
}

std::optional<int64_t> CharacterExtractor::readLongProperty(uint64_t entityAddress, uint32_t propId) {
    auto val = readPropertyValue(entityAddress, propId);
    if (!val) return std::nullopt;
    
    return static_cast<int64_t>(*val);
}

std::optional<float> CharacterExtractor::readFloatProperty(uint64_t entityAddress, uint32_t propId) {
    auto val = readPropertyValue(entityAddress, propId);
    if (!val) return std::nullopt;
    
    // The property value is stored as float bits in the lower 32 bits
    uint32_t floatBits = static_cast<uint32_t>(*val);
    float result;
    std::memcpy(&result, &floatBits, sizeof(float));
    return result;
}

std::vector<int> CharacterExtractor::readArrayProperty(uint64_t entityAddress, uint32_t propId) {
    std::vector<int> result;
    
    auto rawVal = readPropertyValue(entityAddress, propId);
    if (!rawVal || *rawVal == 0) return result;
    
    uint64_t arrayPtr = *rawVal;
    
    // ARRAY property value is a pointer to a ref-counted smart array:
    // +0..+15: refCountTemplateSize (ptrSize + intSize = 16 for 64-bit)
    // +16:     data pointer (ptrSize bytes) → points to array elements
    // +24:     capacity (4 bytes)
    // +28:     nbItems (4 bytes)
    int refCountSize = m_config.is64Bit ? 16 : 8;
    int ptrSize = m_config.is64Bit ? 8 : 4;
    int headerSize = refCountSize + ptrSize + 4 + 4;  // refcount + dataPtr + capacity + count
    
    auto headerBuf = m_memory->readMemory(arrayPtr, headerSize);
    if (!headerBuf) return result;
    
    uint64_t dataPtr = headerBuf->readPointer(refCountSize, true);
    uint32_t nbItems = headerBuf->read<uint32_t>(refCountSize + ptrSize + 4);
    
    if (dataPtr == 0 || nbItems == 0 || nbItems > 10000) return result;
    
    // Each element is 2 * ptrSize bytes:
    //   +0: descriptor pointer (type info)
    //   +ptrSize: value (for INT elements, read as uint32)
    int elementSize = 2 * ptrSize;
    auto dataBuf = m_memory->readMemory(dataPtr, nbItems * elementSize);
    if (!dataBuf) return result;
    
    for (uint32_t i = 0; i < nbItems; i++) {
        uint32_t value = dataBuf->read<uint32_t>(i * elementSize + ptrSize);
        result.push_back(static_cast<int>(value));
    }
    
    return result;
}

// ============================================================================
// WSL References Table - Title Extraction
// ============================================================================
// Based on decompiled LOTRO Companion 6.4 (Java):
//   ReferencesTableController.java, WSLDecoder.java, WslInspector.java,
//   NativePackagesDecoder.java, ContainersDecoder.java, HashtableDecoder.java
// ============================================================================

std::vector<CharacterExtractor::WSLRefEntry> CharacterExtractor::readReferencesTable() {
    std::vector<WSLRefEntry> entries;
    if (!isConnected()) return entries;
    
    int ptrSize = m_config.is64Bit ? 8 : 4;
    int intSize = m_config.is64Bit ? 8 : 4;  // intSize from Java config
    
    // referencesTableAddress is an offset from module base
    uint64_t refTableOffset = m_config.is64Bit ? 0x1dd4138 : 0x1d2e5f4;
    
    std::string moduleName = m_config.is64Bit ? "lotroclient64.exe" : "lotroclient.exe";
    auto modInfo = m_memory->getModuleEx(moduleName);
    if (!modInfo) {
        spdlog::warn("Failed to get module info for WSL references table");
        return entries;
    }
    
    uint64_t refTableAddr = modInfo->baseAddress + refTableOffset;
    
    // Read pointer at referencesTableAddress → table handle
    auto ptrBuf = m_memory->readMemory(refTableAddr, ptrSize);
    if (!ptrBuf) {
        spdlog::warn("Failed to read references table pointer at 0x{:X}", refTableAddr);
        return entries;
    }
    uint64_t tableHandle = ptrBuf->readPointer(0, true);
    if (tableHandle == 0) {
        spdlog::warn("References table handle is null");
        return entries;
    }
    
    // From tableHandle, read: tablePointer(ptrSize) + nbEntries(+ptrSize+4) + gcGeneration(+ptrSize+12)
    auto handleBuf = m_memory->readMemory(tableHandle, 120);
    if (!handleBuf) {
        spdlog::warn("Failed to read references table handle at 0x{:X}", tableHandle);
        return entries;
    }
    
    uint64_t tablePointer = handleBuf->readPointer(0, true);
    int nbEntries = handleBuf->read<int32_t>(ptrSize + 4);
    int nbUsedEntries = handleBuf->read<int32_t>(ptrSize + 8);
    int gcGeneration = handleBuf->read<int32_t>(ptrSize + 12) & 0xFF;
    
    spdlog::info("WSL References Table: {} entries ({} used), gcGeneration={}", 
                 nbEntries, nbUsedEntries, gcGeneration);
    
    if (nbEntries <= 0 || nbEntries > 100000) {
        spdlog::warn("Invalid references table size: {}", nbEntries);
        return entries;
    }
    
    // Read array of entry pointers
    auto tableBuf = m_memory->readMemory(tablePointer, nbEntries * ptrSize);
    if (!tableBuf) {
        spdlog::warn("Failed to read references table entries");
        return entries;
    }
    
    // For each entry pointer, read the entry data
    int entrySize = intSize + 3 * ptrSize;  // bitfield(intSize) + factory + wsl + native pointers
    int loaded = 0;
    
    for (int i = 0; i < nbEntries; i++) {
        uint64_t entryPtr = tableBuf->readPointer(i * ptrSize, true);
        if (entryPtr == 0) continue;
        
        auto entryBuf = m_memory->readMemory(entryPtr, entrySize);
        if (!entryBuf) continue;
        
        int bitfield = entryBuf->read<int32_t>(0);
        int entryGcGen = bitfield & 0xFF;
        if (entryGcGen != gcGeneration) continue;
        
        uint64_t factoryPtr = entryBuf->readPointer(intSize, true);
        uint64_t wslPtr = entryBuf->readPointer(intSize + ptrSize, true);
        uint64_t nativePtr = entryBuf->readPointer(intSize + 2 * ptrSize, true);
        
        if (factoryPtr == 0) continue;
        
        // Read packageID from factory info
        auto factBuf = m_memory->readMemory(factoryPtr, 4);
        if (!factBuf) continue;
        
        int packageID = factBuf->read<int32_t>(0);
        
        WSLRefEntry entry;
        entry.index = i;
        entry.packageID = packageID;
        entry.bitfield = bitfield;
        entry.factoryPtr = factoryPtr;
        entry.wslPtr = wslPtr;
        entry.nativePtr = nativePtr;
        entries.push_back(entry);
        loaded++;
    }
    
    spdlog::info("Loaded {} references table entries", loaded);
    return entries;
}

std::vector<int> CharacterExtractor::parseWSLReferences(uint64_t factoryPtr, uint64_t wslPtr) {
    std::vector<int> references;
    if (wslPtr == 0) return references;
    
    int ptrSize = m_config.is64Bit ? 8 : 4;
    
    // Read factory info: packageID(4) + slots(4) + smartArraySize × 3
    int smartArraySize = ptrSize + 4 + 4;
    int factorySize = 8 + ptrSize + 3 * smartArraySize;
    auto factBuf = m_memory->readMemory(factoryPtr, factorySize);
    if (!factBuf) return references;
    
    int numSlots = factBuf->read<int32_t>(4);
    if (numSlots <= 0 || numSlots > 1000) return references;
    
    // Read the WSL buffer: numSlots × 8 bytes
    // Each attribute is value(4) + typeCode(4) = 8 bytes for basic types
    // DOUBLE/LONG have an extra typeCode + value2, making them 12 bytes
    int wslBufSize = numSlots * 8;
    auto wslBuf = m_memory->readMemory(wslPtr, wslBufSize);
    if (!wslBuf) return references;
    
    // Parse attributes - scan for typeCode = 1 (REFERENCE)
    int offset = 0;
    while (offset + 8 <= wslBufSize) {
        int value = wslBuf->read<int32_t>(offset);
        offset += 4;
        
        int typeCode = wslBuf->read<int32_t>(offset);
        offset += 4;
        
        if (typeCode == 1) {
            // REFERENCE type - value is an index into the references table
            if (value > 0) {
                references.push_back(value);
            }
        } else if (typeCode == 130 || typeCode == 131 || typeCode == 132 || typeCode == 195) {
            // LONG or DOUBLE - has an extra 4 bytes (second half of the 64-bit value)  
            // But we need to handle this carefully: the structure is:
            // value1(4) + typeCode(4) + value2(4) + finalTypeCode(4)
            // The first typeCode is a "mid" type code, and there's a final one too
            // We already consumed value1 + typeCode above
            // Skip value2(4) + finalTypeCode(4)
            offset += 4; // value2
            if (offset + 4 <= wslBufSize) {
                offset += 4; // final typeCode
            }
        }
        // For INTEGER (2,4) and FLOAT (3,4), the 8 bytes we already read is complete
    }
    
    return references;
}

std::map<int, int> CharacterExtractor::decodeNativeIntIntHashtable(uint64_t nativePtr) {
    std::map<int, int> result;
    if (nativePtr == 0) return result;
    
    int ptrSize = m_config.is64Bit ? 8 : 4;
    
    // Hashtable layout (from HashtableDecoder.java):
    // +0:          (2 × ptrSize reserved - prev/next or metadata)
    // +2×ptrSize:  bucketsPointer
    // +3×ptrSize:  firstBucketPointer (unused)
    // +4×ptrSize:  nbBuckets (4 bytes)
    // +4×ptrSize+4: nbElements (4 bytes)
    int hashTableHeaderSize = 4 * ptrSize + 8;
    auto headerBuf = m_memory->readMemory(nativePtr, hashTableHeaderSize);
    if (!headerBuf) return result;
    
    uint64_t bucketsPtr = headerBuf->readPointer(2 * ptrSize, true);
    int nbBuckets = headerBuf->read<int32_t>(4 * ptrSize);
    int nbElements = headerBuf->read<int32_t>(4 * ptrSize + 4);
    
    if (bucketsPtr == 0 || nbBuckets <= 0 || nbBuckets > 100000) return result;
    if (nbElements <= 0 || nbElements > 100000) return result;
    
    spdlog::debug("IntInt hashtable: {} buckets, {} elements", nbBuckets, nbElements);
    
    // Read buckets array
    auto bucketsBuf = m_memory->readMemory(bucketsPtr, nbBuckets * ptrSize);
    if (!bucketsBuf) return result;
    
    // For IntInt hash (packageID 11/35), key layout:
    // keySize = 16 (64-bit) or 12 (32-bit) — from ContainersDecoder
    // valueSize = 12 (64-bit) or 4 (32-bit)
    // Entry layout: key(keySize) + nextPointer(ptrSize) + value(valueSize)
    int keySize = m_config.is64Bit ? 16 : 12;
    int valueOffset = keySize + ptrSize;
    int valueSize = m_config.is64Bit ? 12 : 4;
    int entrySize = valueOffset + valueSize;
    
    for (int b = 0; b < nbBuckets; b++) {
        uint64_t entryPtr = bucketsBuf->readPointer(b * ptrSize, true);
        
        // Follow linked list
        int chainLen = 0;
        while (entryPtr != 0 && chainLen < 1000) {
            auto entryBuf = m_memory->readMemory(entryPtr, entrySize);
            if (!entryBuf) break;
            
            int key = entryBuf->read<int32_t>(0);
            int value = entryBuf->read<int32_t>(valueOffset);
            uint64_t nextPtr = entryBuf->readPointer(keySize, true);
            
            result[key] = value;
            
            entryPtr = nextPtr;
            chainLen++;
        }
    }
    
    if (static_cast<int>(result.size()) != nbElements) {
        spdlog::debug("Hashtable size mismatch: got {}, expected {}", result.size(), nbElements);
    }
    
    return result;
}

std::vector<int> CharacterExtractor::extractTitlesFromWSL() {
    std::vector<int> titleIds;
    
    spdlog::info("=== Extracting titles from WSL References Table ===");
    
    auto entries = readReferencesTable();
    if (entries.empty()) {
        spdlog::warn("No references table entries found");
        return titleIds;
    }
    
    // Step 1: Find PlayerAvatar entry (packageID 1654)
    const WSLRefEntry* avatarEntry = nullptr;
    for (const auto& entry : entries) {
        if (entry.packageID == 1654) {
            // In Java: check m_rcPedigreeRegistry attribute is valid
            // We'll accept the first match for now
            avatarEntry = &entry;
            spdlog::info("Found PlayerAvatar entry at index {}", entry.index);
            break;
        }
    }
    
    if (!avatarEntry) {
        spdlog::warn("PlayerAvatar (packageID 1654) not found in references table");
        return titleIds;
    }
    
    // Step 2: Parse WSL data for PlayerAvatar to extract REFERENCE values
    auto avatarRefs = parseWSLReferences(avatarEntry->factoryPtr, avatarEntry->wslPtr);
    spdlog::info("PlayerAvatar has {} REFERENCE attributes", avatarRefs.size());
    
    // Step 3: Build index of all entries by their table index for lookup
    std::map<int, const WSLRefEntry*> entryByIndex;
    for (const auto& entry : entries) {
        entryByIndex[entry.index] = &entry;
    }
    
    // Step 4: Follow avatar references to find title registry
    // The title registry is a ClassInstance (WSL entry) that itself contains
    // m_rhTitles — a REFERENCE to a native IntIntHashTable
    for (int refIdx : avatarRefs) {
        auto it = entryByIndex.find(refIdx);
        if (it == entryByIndex.end()) continue;
        
        const WSLRefEntry* refEntry = it->second;
        
        // Check if this reference has sub-references (it's a WSL ClassInstance)
        if (refEntry->wslPtr == 0) continue;
        
        auto subRefs = parseWSLReferences(refEntry->factoryPtr, refEntry->wslPtr);
        
        for (int subRefIdx : subRefs) {
            auto subIt = entryByIndex.find(subRefIdx);
            if (subIt == entryByIndex.end()) continue;
            
            const WSLRefEntry* subRefEntry = subIt->second;
            
            // Check if this is a native entry (the titles hashtable is native)
            bool isNative = (subRefEntry->bitfield & 0x10000000) != 0;
            if (!isNative || subRefEntry->nativePtr == 0) continue;
            
            // Read native packageID to check if it's an IntIntHashTable (11 or 35)
            auto nativeFact = m_memory->readMemory(subRefEntry->factoryPtr, 12);
            if (!nativeFact) continue;
            
            int nativePackageID = nativeFact->read<int32_t>(0);
            
            if (nativePackageID != 11 && nativePackageID != 35) continue;
            
            // This is an IntInt hashtable - decode it
            auto hashMap = decodeNativeIntIntHashtable(subRefEntry->nativePtr);
            
            if (hashMap.empty()) continue;
            
            // Check if keys look like title DIDs (typically > 0x70000000 / 1879048192)
            int titleLikeCount = 0;
            for (const auto& [key, value] : hashMap) {
                if (key > 0x70000000) titleLikeCount++;
            }
            
            if (titleLikeCount > 0 && titleLikeCount >= static_cast<int>(hashMap.size()) / 2) {
                spdlog::info("Found titles hashtable at ref {} -> sub-ref {} (native pkg {}): {} entries, {} title-like DIDs",
                             refIdx, subRefIdx, nativePackageID, hashMap.size(), titleLikeCount);
                
                for (const auto& [titleDID, acquisitionData] : hashMap) {
                    if (titleDID > 0) {
                        titleIds.push_back(titleDID);
                    }
                }
                
                spdlog::info("Extracted {} title IDs from WSL references table", titleIds.size());
                return titleIds;
            }
        }
    }
    
    spdlog::info("No titles hashtable found in avatar references (searched {} refs)", avatarRefs.size());
    return titleIds;
}

bool CharacterExtractor::scanPatterns() {
    if (!isConnected()) return false;
    
    spdlog::info("Scanning for memory patterns...");
    
    std::string moduleName = m_config.is64Bit ? "lotroclient64.exe" : "lotroclient.exe";
    
    // Get module info
    auto modInfo = m_memory->getModuleEx(moduleName);
    if (!modInfo) {
        spdlog::error("Failed to get module info for scanning");
        return false;
    }
    
    // Read module content (limit to e.g. 60MB to cover code section)
    size_t scanSize = std::min<size_t>(modInfo->size, 60 * 1024 * 1024);
    auto buffer = m_memory->readMemory(modInfo->baseAddress, scanSize);
    if (!buffer) {
        spdlog::error("Failed to read module memory for scanning");
        return false;
    }
    
    // Copy data to vector for scanner
    std::vector<uint8_t> data(buffer->data(), buffer->data() + buffer->size());
    
    bool allFound = true;
    
    if (m_config.is64Bit) {
        // 64-bit patterns
        // Entities Table: 48895c2408574883ec40488bd9488b0d?3
        auto entitiesIdx = PatternScanner::find(data, "48895c2408574883ec40488bd9488b0d?3");
        if (entitiesIdx) {
            // Offset calculation matching Java: index + 16 (offset to ?3) + 4 + value
            size_t instrOffset = *entitiesIdx + 16;
            int32_t relOffset = buffer->read<int32_t>(instrOffset);
            uint64_t finalAddr = modInfo->baseAddress + instrOffset + 4 + relOffset;
            
            m_config.entitiesTableOffset = finalAddr - m_config.baseAddress; // Store relative offset
            spdlog::info("Found Entities Table: 0x{:X} (Offset: 0x{:X})", finalAddr, m_config.entitiesTableOffset);
        } else {
            spdlog::error("Entities Table pattern not found");
            allFound = false;
        }
        
        // Client Data: 48893d?3b201b900010000
        auto clientIdx = PatternScanner::find(data, "48893d?3b201b900010000");
        if (clientIdx) {
            // Offset calculation: index + 3 + 4 + value
            size_t instrOffset = *clientIdx + 3;
            int32_t relOffset = buffer->read<int32_t>(instrOffset);
            uint64_t finalAddr = modInfo->baseAddress + instrOffset + 4 + relOffset;
            
            m_config.clientDataOffset = finalAddr - m_config.baseAddress;
            // Access account data at same address as client data (per config)
            m_config.accountDataOffset = m_config.clientDataOffset;
            
            spdlog::info("Found Client Data: 0x{:X} (Offset: 0x{:X})", finalAddr, m_config.clientDataOffset);
        } else {
            spdlog::error("Client Data pattern not found");
            allFound = false;
        }
        
        // Storage Data: 4883EC28BA02000000488D0D?3
        auto storageIdx = PatternScanner::find(data, "4883EC28BA02000000488D0D?3");
        if (storageIdx) {
            // Offset calculation: index + 12 + 4 + value
            size_t instrOffset = *storageIdx + 12;
            int32_t relOffset = buffer->read<int32_t>(instrOffset);
            uint64_t finalAddr = modInfo->baseAddress + instrOffset + 4 + relOffset;
            
            m_config.storageDataOffset = finalAddr - m_config.baseAddress;
            spdlog::info("Found Storage Data: 0x{:X} (Offset: 0x{:X})", finalAddr, m_config.storageDataOffset);
        } else {
            spdlog::error("Storage Data pattern not found");
            allFound = false;
        }
        
    } else {
        // 32-bit patterns
        // Entities: 8B0D?383EC?05633F63BCE
        // ... (Implement if needed, focusing on 64-bit for now as user is on 64-bit)
        spdlog::warn("32-bit pattern scanning not fully implemented yet");
        // We can reuse hardcoded if scanning not implemented
    }
    
    return allFound;
}

void CharacterExtractor::debugDumpProperties(uint64_t entityAddress) {
    if (!isConnected()) return;

    spdlog::info("--- PROPERTY DUMP START (Entity 0x{:X}) ---", entityAddress);

    // Get EPP
    uint64_t eppOffset = m_config.is64Bit ? 192 : 108;
    auto entBuf = m_memory->readMemory(entityAddress, 256);
    if (!entBuf) return;
    
    uint64_t eppPtr = entBuf->readPointer(eppOffset, true);
    if (eppPtr == 0) return;
    
    auto eppBuf = m_memory->readMemory(eppPtr, 128);
    if (!eppBuf) return;
    
    uint64_t bucketsPtr = eppBuf->readPointer(72, true);
    uint32_t nbBuckets = eppBuf->read<uint32_t>(88);
    
    if (bucketsPtr == 0 || nbBuckets == 0 || nbBuckets > 10000) return;
    
    auto bucketsBuf = m_memory->readMemory(bucketsPtr, nbBuckets * 8);
    if (!bucketsBuf) return;
    
    for (uint32_t i = 0; i < nbBuckets; i++) {
        uint64_t nodePtr = bucketsBuf->readPointer(i * 8, true);
        
        while (nodePtr != 0) {
            auto nodeBuf = m_memory->readMemory(nodePtr, 32);
            if (!nodeBuf) break;
            
            uint32_t id = nodeBuf->read<uint32_t>(0);
            uint64_t value = nodeBuf->read<uint64_t>(24);
            uint64_t nextPtr = nodeBuf->readPointer(8, true);
            
            // Get Property Name
            std::string propName = "Unknown";
            if (m_datFacade) {
                auto* registry = m_datFacade->getPropertiesRegistry();
                if (registry) {
                     auto def = registry->getPropertyDef(id);
                     if (def) propName = def->name().toStdString();
                }
            }
            
            // Check if value looks like a pointer to string
            bool stringFound = false;
            if (value > 0x10000 && value < 0x7FFFFFFFFFFF) {
                auto valObjBuf = m_memory->readMemory(value, 32);
                if (valObjBuf) {
                    // Check offsets 0, 12, 16 for strings
                    std::vector<int> offsets = {0, 12, 16};
                    for (int off : offsets) {
                        uint64_t strPtr = valObjBuf->readPointer(off, true);
                        if (strPtr > 0x10000 && strPtr < 0x7FFFFFFFFFFF) {
                            // Try Latin-1
                            auto s = m_memory->readString(strPtr, 64);
                            if (s && s->length() > 1 && s->length() < 100) {
                                bool printable = true;
                                for(char c : *s) if((unsigned char)c < 32 || (unsigned char)c > 126) printable = false;
                                if(printable) {
                                     spdlog::info("Prop ID {} ({}): Val=0x{:X} -> Str({})='{}'", id, propName, value, off, *s);
                                     stringFound = true;
                                }
                            }
                            // Try Wide
                            if (!stringFound) {
                                auto ws = m_memory->readWideString(strPtr, 64);
                                if (ws && ws->length() > 1 && ws->length() < 100) {
                                     spdlog::info("Prop ID {} ({}): Val=0x{:X} -> WStr({})='{}'", id, propName, value, off, QString::fromStdWString(*ws).toStdString());
                                     stringFound = true;
                                }
                            }
                        }
                    }
                }
            }
            
            nodePtr = nextPtr;
        }
    }
    
    spdlog::info("--- PROPERTY DUMP END ---");
}

} // namespace lotro
