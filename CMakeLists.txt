# CMake Project for lotro-launcher
# Managed by Conan (was vcpkg)
cmake_minimum_required(VERSION 3.21)

# Project definition
project(lotro-launcher
    VERSION 0.1.0

    DESCRIPTION "Cross-platform LOTRO Launcher with mod management"
    LANGUAGES CXX
)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type defaults
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    add_compile_definitions(PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LINUX TRUE)
    add_compile_definitions(PLATFORM_LINUX)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
    add_compile_definitions(PLATFORM_MACOS)
endif()

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Xml
    Concurrent
)

# Automatically handle Qt MOC, UIC, RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find other dependencies
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Linux-specific dependencies
if(PLATFORM_LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
endif()

# Source files
set(CORE_SOURCES
    src/main.cpp
    src/core/config/ConfigManager.cpp
    src/core/config/GameConfig.cpp
    src/core/config/AccountConfig.cpp
    src/core/config/WineConfig.cpp
    src/core/platform/Platform.cpp
    src/core/credentials/CredentialStore.cpp
    src/core/JournalManager.cpp
)

set(NETWORK_SOURCES
    src/network/SoapClient.cpp
    src/network/GameServicesInfo.cpp
    src/network/LoginAccount.cpp
    src/network/WorldList.cpp
    src/network/NewsfeedParser.cpp
    src/network/LotroInterfaceClient.cpp
)

set(GAME_SOURCES
    src/game/GameLauncher.cpp
    src/game/PatchClient.cpp
    src/game/NativePatcher.cpp
    src/game/DatFile.cpp
    src/game/PatchServerClient.cpp
    src/game/LaunchArguments.cpp
    src/game/UserPreferences.cpp
)

set(ADDONS_SOURCES
    src/addons/AddonManager.cpp
    src/addons/CompendiumParser.cpp
)

# Companion sources (deferred to later phase)
# set(COMPANION_SOURCES
#     src/companion/CharacterTracker.cpp
#     src/companion/GearSimulator.cpp
#     src/companion/GameDatabase.cpp
# )
set(COMPANION_SOURCES
    src/companion/ProcessMemory.cpp
    src/companion/CharacterExtractor.cpp
    src/companion/CharacterTracker.cpp
    src/companion/GameDatabase.cpp
    src/companion/ItemDatabase.cpp
    src/companion/StatCalculator.cpp
    src/companion/LiveSyncService.cpp
    src/companion/PatternScanner.cpp
    src/companion/export/DataExporter.cpp
)

# DAT file utilities (ported from lotro-companion)
set(DAT_SOURCES
    src/dat/BufferUtils.cpp
    src/dat/DatArchive.cpp
    src/dat/PropertyDefinitionsLoader.cpp
    src/dat/DataFacade.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/LoginWidget.cpp
    src/ui/SettingsWindow.cpp
    src/ui/AddonManagerWindow.cpp
    src/ui/SetupWizard.cpp
    src/ui/CharacterTrackerWindow.cpp
    src/ui/CharacterListWidget.cpp
    src/ui/DeedBrowserWidget.cpp
    src/ui/RecipeBrowserWidget.cpp
    src/ui/GearSimulatorWidget.cpp
    src/ui/SyncStatusWidget.cpp
    src/ui/CompanionWindow.cpp
    src/ui/DataExportWindow.cpp
    src/ui/PatchDialog.cpp
    src/ui/JournalWindow.cpp
)

# Linux-only Wine and Steam sources
if(PLATFORM_LINUX)
    set(WINE_SOURCES
        src/wine/WineManager.cpp
        src/wine/WinePrefixSetup.cpp
        src/wine/WineProcessBuilder.cpp
    )
    set(STEAM_SOURCES
        src/steam/SteamIntegration.cpp
    )
else()
    set(WINE_SOURCES "")
    set(STEAM_SOURCES "")
endif()

# Platform-specific sources
if(PLATFORM_LINUX)
    list(APPEND CORE_SOURCES src/core/platform/LinuxPlatform.cpp)
    list(APPEND CORE_SOURCES src/core/credentials/LibSecretStore.cpp)
elseif(PLATFORM_WINDOWS)
    list(APPEND CORE_SOURCES src/core/platform/WindowsPlatform.cpp)
    list(APPEND CORE_SOURCES src/core/credentials/WindowsCredentialStore.cpp)
endif()

# Qt Resources
set(RESOURCES
    resources/resources.qrc
)

# Create executable
add_executable(${PROJECT_NAME}
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${GAME_SOURCES}
    ${ADDONS_SOURCES}
    ${COMPANION_SOURCES}
    ${DAT_SOURCES}
    ${UI_SOURCES}
    ${WINE_SOURCES}
    ${STEAM_SOURCES}
    ${RESOURCES}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Xml
    Qt6::Concurrent
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    z  # zlib for DAT file decompression
)

# Linux-specific linking
if(PLATFORM_LINUX)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBSECRET_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBSECRET_LIBRARIES} dl)
endif()

# Windows-specific linking
if(PLATFORM_WINDOWS)
    target_link_libraries(${PROJECT_NAME} PRIVATE Crypt32 Advapi32)
endif()

# Install rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Install patch client wrapper (only if it exists - needs separate compilation with mingw)
if(EXISTS "${CMAKE_SOURCE_DIR}/resources/bin/run_patch_client.exe")
    install(FILES ${CMAKE_SOURCE_DIR}/resources/bin/run_patch_client.exe
        DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endif()

# Testing support
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
