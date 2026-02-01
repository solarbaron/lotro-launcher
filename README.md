# LOTRO Launcher

A cross-platform launcher for **Lord of the Rings Online** with mod management and LOTRO Companion-like features.

## Features

- 🐧 **Linux Native** - Uses Wine with configurable prefix (similar to XIVLauncher)
- 🪟 **Windows Native** - No Wine needed on Windows
- 🔌 **Mod Manager** - Plugins, skins, and music from lotrointerface.com
- 👤 **Multi-Account** - Save and switch between accounts
- 📊 **Character Tracking** - LOTRO Companion-inspired features
- 🔐 **Secure Credentials** - Uses system keyring (libsecret/Windows Credential Manager)

## Building

### Prerequisites

- CMake 3.21+
- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022)
- Qt6 (Core, Widgets, Network, Xml, Concurrent)
- vcpkg (for dependency management)

### Linux

```bash
# Install system dependencies (Ubuntu/Debian)
sudo apt install build-essential cmake qt6-base-dev libsecret-1-dev

# Clone and build
git clone https://github.com/solarbaron/lotro-launcher.git
cd lotro-launcher
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Windows

```powershell
# Using vcpkg
git clone https://github.com/Microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install qtbase:x64-windows spdlog:x64-windows nlohmann-json:x64-windows

# Build
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## Configuration

Configuration files are stored in:
- **Linux**: `~/.config/lotro-launcher/`
- **Windows**: `%APPDATA%\lotro-launcher\`

### Wine Settings (Linux)

The launcher can either manage its own Wine installation or use a user-provided Wine prefix:

- **Built-in Mode**: Downloads and configures Wine + DXVK automatically
- **Custom Mode**: Point to your existing Wine installation and prefix

## Credits

- Based on [OneLauncher](https://github.com/JuneStepp/OneLauncher) by June Stepp
- LOTRO is a trademark of Middle-earth Enterprises
- Lord of the Rings Online is owned by Standing Stone Games LLC

## License

GPL-3.0-or-later
