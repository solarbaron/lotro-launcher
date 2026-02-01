/**
 * Test DatFile Reader
 */

#include "game/DatFile.hpp"

#include <QCoreApplication>
#include <iostream>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        std::cout << "Usage: test_datfile <game_directory>\n";
        std::cout << "Example: test_datfile \"/home/user/.steam/steamapps/common/Lord of the Rings Online\"\n";
        return 1;
    }

    std::filesystem::path gameDir = argv[1];

    std::cout << "Scanning .dat files in: " << gameDir << "\n\n";

    auto versions = lotro::scanDatVersions(gameDir);

    std::cout << "Found " << versions.size() << " .dat files:\n";
    std::cout << std::string(80, '-') << "\n";
    std::cout << std::left << std::setw(35) << "File" 
              << std::setw(12) << "Version" 
              << std::setw(15) << "Max File Ver"
              << std::setw(12) << "Files" << "\n";
    std::cout << std::string(80, '-') << "\n";

    for (const auto& info : versions) {
        std::cout << std::left << std::setw(35) << info.datName.toStdString()
                  << std::setw(12) << info.version
                  << std::setw(15) << info.maxFileVersion
                  << std::setw(12) << info.fileCount << "\n";
    }

    std::cout << std::string(80, '-') << "\n";

    // Test opening one .dat file in detail
    if (!versions.empty()) {
        std::cout << "\nDetailed info for: " << versions[0].datName.toStdString() << "\n";
        
        lotro::DatFile dat(versions[0].datPath);
        if (dat.isValid()) {
            const auto& sb = dat.superblock();
            std::cout << "  Block size: " << sb.blockSize << "\n";
            std::cout << "  File size: " << sb.fileSize << " bytes\n";
            std::cout << "  Version: " << sb.version << "\n";
            std::cout << "  Version2: " << sb.version2 << "\n";
            std::cout << "  Directory offset: 0x" << std::hex << sb.directoryOffset << std::dec << "\n";
            
            // Show first few file entries
            const auto& entries = dat.fileEntries();
            std::cout << "\n  First 5 file entries:\n";
            for (size_t i = 0; i < std::min(size_t(5), entries.size()); i++) {
                const auto& e = entries[i];
                std::cout << "    [" << i << "] ID=0x" << std::hex << e.fileId 
                          << " offset=0x" << e.fileOffset
                          << std::dec << " size=" << e.size 
                          << " version=" << e.version << "\n";
            }
        } else {
            std::cout << "  Error: " << dat.lastError().toStdString() << "\n";
        }
    }

    return 0;
}
