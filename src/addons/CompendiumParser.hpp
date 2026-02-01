/**
 * LOTRO Launcher - Compendium Parser
 * 
 * Parser for addon compendium XML files.
 * Based on OneLauncher's addon parsing logic.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <QString>

#include "AddonManager.hpp"

namespace lotro {

/**
 * Compendium file type
 */
enum class CompendiumType {
    Plugin,    // .plugincompendium
    Skin,      // .skincompendium
    Music      // .musiccompendium
};

/**
 * Compendium file parser
 * 
 * Parses the XML compendium files that describe addons.
 * 
 * Format:
 * <PluginConfig>
 *     <Id>12345</Id>
 *     <Name>Plugin Name</Name>
 *     <Version>1.0.0</Version>
 *     <Author>Author Name</Author>
 *     <Description>Description</Description>
 *     <InfoUrl>http://lotrointerface.com/downloads/info12345</InfoUrl>
 *     <DownloadUrl>http://lotrointerface.com/downloads/download12345</DownloadUrl>
 *     <Descriptors>
 *         <descriptor>Author\Plugin.plugin</descriptor>
 *     </Descriptors>
 *     <Dependencies>
 *         <dependency>0</dependency>
 *     </Dependencies>
 *     <StartupScript>Author\script.py</StartupScript>
 * </PluginConfig>
 */
class CompendiumParser {
public:
    /**
     * Parse a compendium file and return addon info
     * 
     * @param path Path to the compendium file
     * @return AddonInfo if parsing succeeded
     */
    static std::optional<AddonInfo> parse(const std::filesystem::path& path);
    
    /**
     * Parse compendium XML content
     * 
     * @param content XML content
     * @param type Compendium type
     * @return AddonInfo if parsing succeeded
     */
    static std::optional<AddonInfo> parseContent(
        const QString& content,
        CompendiumType type
    );
    
    /**
     * Determine compendium type from file extension
     */
    static std::optional<CompendiumType> getTypeFromPath(
        const std::filesystem::path& path
    );
    
    /**
     * Get the expected file extension for a compendium type
     */
    static QString getExtension(CompendiumType type);
    
    /**
     * Find all compendium files in a directory
     * 
     * @param directory Directory to search
     * @param recursive Search subdirectories
     * @return List of compendium file paths
     */
    static std::vector<std::filesystem::path> findCompendiumFiles(
        const std::filesystem::path& directory,
        bool recursive = true
    );
    
    /**
     * Generate a compendium file for an addon
     * 
     * @param info Addon information
     * @return XML content string
     */
    static QString generate(const AddonInfo& info);
};

/**
 * Plugin descriptor (.plugin file) parser
 * 
 * Parses .plugin files found in the Plugins directory.
 */
class PluginDescriptorParser {
public:
    struct PluginDescriptor {
        QString name;
        QString author;
        QString version;
        QString description;
        QString package;       // Lua package name
        QString image;         // Icon image path
    };
    
    /**
     * Parse a .plugin file
     */
    static std::optional<PluginDescriptor> parse(const std::filesystem::path& path);
    
    /**
     * Find all .plugin files in a directory
     */
    static std::vector<std::filesystem::path> findPluginFiles(
        const std::filesystem::path& directory,
        bool recursive = true
    );
};

} // namespace lotro
