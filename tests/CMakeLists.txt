# LOTRO Launcher Tests

if(BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    find_package(Qt6 REQUIRED COMPONENTS Core Test)
    
    # Test executable
    add_executable(lotro-launcher-tests
        test_main.cpp
        test_config.cpp
        test_compendium_parser.cpp
        test_launch_arguments.cpp
    )
    
    target_include_directories(lotro-launcher-tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    
    target_link_libraries(lotro-launcher-tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        Qt6::Core
        Qt6::Test
        nlohmann_json::nlohmann_json
    )
    
    # Register tests with CTest
    include(GoogleTest)
    gtest_discover_tests(lotro-launcher-tests)
endif()
