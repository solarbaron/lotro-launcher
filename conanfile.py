from conan import ConanFile
from conan.tools.cmake import cmake_layout

class LotroLauncher(ConanFile):
    name = "lotro-launcher"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("qt/6.7.3")
        self.requires("spdlog/1.14.1")
        self.requires("nlohmann_json/3.11.3")

    def build_requirements(self):
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)
        
    default_options = {
        "qt/*:shared": True,
        "*:with_mysql": False,
        "*:with_postgresql": False,
        "*:with_odbc": False,
        "*:with_openldap": False,
    }
