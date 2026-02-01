/**
 * LOTRO Launcher - Compendium Parser Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CompendiumParser.hpp"

#include <QFile>
#include <QXmlStreamReader>
#include <QDir>

#include <spdlog/spdlog.h>

namespace lotro {

std::optional<CompendiumType> CompendiumParser::getTypeFromPath(
    const std::filesystem::path& path
) {
    auto ext = path.extension().string();
    
    if (ext == ".plugincompendium") return CompendiumType::Plugin;
    if (ext == ".skincompendium") return CompendiumType::Skin;
    if (ext == ".musiccompendium") return CompendiumType::Music;
    
    return std::nullopt;
}

QString CompendiumParser::getExtension(CompendiumType type) {
    switch (type) {
        case CompendiumType::Plugin: return ".plugincompendium";
        case CompendiumType::Skin:   return ".skincompendium";
        case CompendiumType::Music:  return ".musiccompendium";
    }
    return "";
}

std::optional<AddonInfo> CompendiumParser::parse(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        spdlog::warn("Failed to open compendium file: {}", path.string());
        return std::nullopt;
    }
    
    auto type = getTypeFromPath(path);
    if (!type) {
        spdlog::warn("Unknown compendium type: {}", path.extension().string());
        return std::nullopt;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    return parseContent(content, *type);
}

std::optional<AddonInfo> CompendiumParser::parseContent(
    const QString& content,
    CompendiumType type
) {
    AddonInfo info;
    
    // Set addon type
    switch (type) {
        case CompendiumType::Plugin: info.type = AddonType::Plugin; break;
        case CompendiumType::Skin:   info.type = AddonType::Skin; break;
        case CompendiumType::Music:  info.type = AddonType::Music; break;
    }
    
    QXmlStreamReader reader(content);
    bool inDescriptors = false;
    bool inDependencies = false;
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString();
            
            if (name == "Id") {
                info.id = reader.readElementText();
            } else if (name == "Name") {
                info.name = reader.readElementText();
            } else if (name == "Version") {
                info.version = reader.readElementText();
                info.installedVersion = info.version;
            } else if (name == "Author") {
                info.author = reader.readElementText();
            } else if (name == "Description") {
                info.description = reader.readElementText();
            } else if (name == "InfoUrl") {
                info.infoUrl = reader.readElementText();
            } else if (name == "DownloadUrl") {
                info.downloadUrl = reader.readElementText();
            } else if (name == "Category") {
                info.category = reader.readElementText();
            } else if (name == "StartupScript") {
                info.startupScript = reader.readElementText();
            } else if (name == "Descriptors") {
                inDescriptors = true;
            } else if (name == "Dependencies") {
                inDependencies = true;
            } else if (name == "descriptor" && inDescriptors) {
                info.descriptors.push_back(reader.readElementText());
            } else if (name == "dependency" && inDependencies) {
                QString dep = reader.readElementText();
                if (dep != "0") {  // 0 means no dependency
                    info.dependencies.push_back(dep);
                }
            }
        } else if (reader.isEndElement()) {
            QString name = reader.name().toString();
            if (name == "Descriptors") {
                inDescriptors = false;
            } else if (name == "Dependencies") {
                inDependencies = false;
            }
        }
    }
    
    if (reader.hasError()) {
        spdlog::warn("XML parsing error: {}", reader.errorString().toStdString());
    }
    
    if (info.name.isEmpty()) {
        return std::nullopt;
    }
    
    return info;
}

std::vector<std::filesystem::path> CompendiumParser::findCompendiumFiles(
    const std::filesystem::path& directory,
    bool recursive
) {
    std::vector<std::filesystem::path> files;
    
    if (!std::filesystem::exists(directory)) {
        return files;
    }
    
    try {
        auto iterator = recursive ? 
            std::filesystem::recursive_directory_iterator(directory) :
            std::filesystem::recursive_directory_iterator(directory);
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".plugincompendium" || 
                    ext == ".skincompendium" || 
                    ext == ".musiccompendium") {
                    files.push_back(entry.path());
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error scanning directory: {}", e.what());
    }
    
    return files;
}

QString CompendiumParser::generate(const AddonInfo& info) {
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.setAutoFormatting(true);
    
    QString rootElement;
    switch (info.type) {
        case AddonType::Plugin: rootElement = "PluginConfig"; break;
        case AddonType::Skin:   rootElement = "SkinConfig"; break;
        case AddonType::Music:  rootElement = "MusicConfig"; break;
    }
    
    writer.writeStartDocument();
    writer.writeStartElement(rootElement);
    
    writer.writeTextElement("Id", info.id);
    writer.writeTextElement("Name", info.name);
    writer.writeTextElement("Version", info.version);
    writer.writeTextElement("Author", info.author);
    writer.writeTextElement("Description", info.description);
    writer.writeTextElement("InfoUrl", info.infoUrl);
    writer.writeTextElement("DownloadUrl", info.downloadUrl);
    
    if (!info.descriptors.empty()) {
        writer.writeStartElement("Descriptors");
        for (const auto& desc : info.descriptors) {
            writer.writeTextElement("descriptor", desc);
        }
        writer.writeEndElement();
    }
    
    if (!info.dependencies.empty()) {
        writer.writeStartElement("Dependencies");
        for (const auto& dep : info.dependencies) {
            writer.writeTextElement("dependency", dep);
        }
        writer.writeEndElement();
    }
    
    if (!info.startupScript.isEmpty()) {
        writer.writeTextElement("StartupScript", info.startupScript);
    }
    
    writer.writeEndElement();
    writer.writeEndDocument();
    
    return xml;
}

// Plugin descriptor parser

std::optional<PluginDescriptorParser::PluginDescriptor> 
PluginDescriptorParser::parse(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    
    PluginDescriptor desc;
    QXmlStreamReader reader(&file);
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString();
            
            if (name == "Name") {
                desc.name = reader.readElementText();
            } else if (name == "Author") {
                desc.author = reader.readElementText();
            } else if (name == "Version") {
                desc.version = reader.readElementText();
            } else if (name == "Description") {
                desc.description = reader.readElementText();
            } else if (name == "Package") {
                desc.package = reader.readElementText();
            } else if (name == "Image") {
                desc.image = reader.readElementText();
            }
        }
    }
    
    if (desc.name.isEmpty()) {
        return std::nullopt;
    }
    
    return desc;
}

std::vector<std::filesystem::path> PluginDescriptorParser::findPluginFiles(
    const std::filesystem::path& directory,
    bool recursive
) {
    std::vector<std::filesystem::path> files;
    
    if (!std::filesystem::exists(directory)) {
        return files;
    }
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".plugin") {
                files.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error scanning for .plugin files: {}", e.what());
    }
    
    return files;
}

} // namespace lotro
