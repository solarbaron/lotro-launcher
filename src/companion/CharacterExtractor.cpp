/**
 * LOTRO Launcher - Character Extractor Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CharacterExtractor.hpp"
#include "PatternScanner.hpp"

#include <spdlog/spdlog.h>
#include <map>
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
        
        // Read raw property value
        auto rawVal = readPropertyValue(*playerEntity, propId);
        if (!rawVal || *rawVal == 0) continue;
        
        uint64_t rawValue = *rawVal;
        
        // For the first slot, dump detailed diagnostics
        if (slotName == "HEAD") {
            spdlog::info("=== DIAGNOSTIC: {} slot (propId={}) ===", slotName.toStdString(), propId);
            spdlog::info("  Raw 64-bit value: 0x{:016X}", rawValue);
            spdlog::info("  Low 32 bits:  0x{:08X} ({})", static_cast<uint32_t>(rawValue), static_cast<uint32_t>(rawValue));
            spdlog::info("  High 32 bits: 0x{:08X} ({})", static_cast<uint32_t>(rawValue >> 32), static_cast<uint32_t>(rawValue >> 32));
            
            // Try to read the item entity at this address
            // Dump the first 400 bytes looking for the item DID (0x7000xxxx pattern)
            auto entityDump = m_memory->readMemory(rawValue, 400);
            if (entityDump) {
                spdlog::info("  Memory dump at 0x{:X}:", rawValue);
                for (int off = 0; off < 400; off += 8) {
                    uint64_t val64 = entityDump->read<uint64_t>(off);
                    uint32_t valLo = static_cast<uint32_t>(val64);
                    uint32_t valHi = static_cast<uint32_t>(val64 >> 32);
                    // Only log interesting values (non-zero, potential DIDs)
                    if (valLo != 0 && (valLo >= 0x70000000 && valLo < 0x80000000)) {
                        spdlog::info("    +{}: 0x{:08X} <-- potential DID!", off, valLo);
                    }
                    if (valHi != 0 && (valHi >= 0x70000000 && valHi < 0x80000000)) {
                        spdlog::info("    +{}: 0x{:08X} <-- potential DID! (hi32 of +{})", off + 4, valHi, off);
                    }
                }
                // Also dump all uint32 at common offsets
                spdlog::info("  Key offsets (uint32):");
                for (int off : {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248, 256, 264, 272, 280, 288, 296, 304, 312, 320, 328, 336, 344, 352, 360, 368, 376, 384, 392}) {
                    if (off >= 400) break;
                    uint32_t val = entityDump->read<uint32_t>(off);
                    if (val != 0) {
                        spdlog::info("    +{}: 0x{:08X} ({})", off, val, val);
                    }
                }
            } else {
                spdlog::info("  Cannot read memory at 0x{:X} — may not be a valid pointer", rawValue);
            }
        }
        
        // The slot cache value is a pointer to an item entity in memory.
        // Try to follow it and read the item's DataID from ConstructionInfo.
        // ConstructionInfo pointer is at offset 288 (64-bit) / 152 (32-bit) from entity.
        // DataID is at pointerSize + 4 within ConstructionInfo.
        uint64_t ciOffset = m_config.is64Bit ? 288 : 152;
        auto ciPtr = m_memory->readPointer(rawValue + ciOffset);
        if (ciPtr && *ciPtr != 0) {
            int ptrSize = m_config.is64Bit ? 8 : 4;
            auto ciBuf = m_memory->readMemory(*ciPtr, ptrSize + 8);
            if (ciBuf) {
                uint32_t dataId = ciBuf->read<uint32_t>(ptrSize + 4);
                if (dataId != 0 && dataId != 1) {
                    data.equippedGear[slotName] = static_cast<int>(dataId);
                    spdlog::info("Slot {}: entity 0x{:X} -> item DID 0x{:X} ({})", 
                                 slotName.toStdString(), rawValue, dataId, dataId);
                    continue;
                }
            }
        }
        
        // Fallback: store the raw value
        if (rawValue < 0x100000000ULL) {
            data.equippedGear[slotName] = static_cast<int>(rawValue);
            spdlog::info("Slot {}: raw value 0x{:X} ({})", 
                         slotName.toStdString(), rawValue, static_cast<int>(rawValue));
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
    
    // NOTE: Acquired titles are managed server-side and not stored in entity properties
    // or the storage data area. Only the active title (Title_ActiveTitleDID) is accessible
    // from memory. To track all titles, we rely on observing title changes over time.
    
    spdlog::info("Active title: DID {} (0x{:X})", 
                 data.titles.empty() ? 0 : data.titles[0],
                 data.titles.empty() ? 0u : static_cast<uint32_t>(data.titles[0]));
    
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
