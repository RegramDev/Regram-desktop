cmake_minimum_required(VERSION 3.25)
project(RegramPolicyTests LANGUAGES CXX)

find_package(ICU 64 REQUIRED COMPONENTS uc i18n)
enable_testing()

add_executable(regram_policy_tests
    ../core/peer_address.cpp
    ../filters/rules.cpp
    ../filters/matcher.cpp
    ../filters/reveal_policy.cpp
    policy_tests.cpp
)
target_compile_features(regram_policy_tests PRIVATE cxx_std_20)
target_include_directories(regram_policy_tests PRIVATE ../..)
target_link_libraries(regram_policy_tests PRIVATE ICU::uc ICU::i18n)
add_test(NAME regram_policy_tests COMMAND regram_policy_tests)

option(REGRAM_TEST_JSON "Build Qt6 rule exchange tests" OFF)
if (REGRAM_TEST_JSON)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    add_executable(regram_exchange_tests
        ../core/peer_address.cpp
        ../filters/rules.cpp
        ../filters/matcher.cpp
        ../filters/exchange.cpp
        ../filters/scopes.cpp
        ../filters/result_cache.cpp
        exchange_tests.cpp
    )
    target_compile_features(regram_exchange_tests PRIVATE cxx_std_20)
    target_include_directories(regram_exchange_tests PRIVATE ../..)
    target_include_directories(regram_exchange_tests SYSTEM PRIVATE
        ../../../lib_base
        ../../../ThirdParty/GSL/include
    )
    target_link_libraries(regram_exchange_tests PRIVATE
        ICU::uc ICU::i18n Qt6::Core)
    add_test(NAME regram_exchange_tests COMMAND regram_exchange_tests)
endif()

option(REGRAM_TEST_SQLCIPHER "Build encrypted history storage tests" OFF)
if (REGRAM_TEST_SQLCIPHER)
    enable_language(C)
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
    add_library(regram_sqlcipher STATIC ../ayu/libs/sqlite/sqlite3.c)
    target_include_directories(regram_sqlcipher PUBLIC ../ayu/libs/sqlite)
    target_compile_definitions(regram_sqlcipher PUBLIC SQLITE_HAS_CODEC
        PRIVATE SQLITE_TEMP_STORE=2 SQLITE_THREADSAFE=1
        SQLITE_EXTRA_INIT=sqlcipher_extra_init
        SQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown)
    target_link_libraries(regram_sqlcipher PRIVATE OpenSSL::Crypto
        Threads::Threads ${CMAKE_DL_LIBS})
    add_executable(regram_sqlcipher_tests sqlcipher_tests.cpp)
    target_compile_features(regram_sqlcipher_tests PRIVATE cxx_std_20)
    target_link_libraries(regram_sqlcipher_tests PRIVATE regram_sqlcipher)
    add_test(NAME regram_sqlcipher_tests COMMAND regram_sqlcipher_tests)
endif()

if (APPLE)
    find_program(REGRAM_XCRUN xcrun)
    if (REGRAM_XCRUN)
        add_test(NAME regram_foundation_regex
            COMMAND ${REGRAM_XCRUN} swift
                ${CMAKE_CURRENT_SOURCE_DIR}/foundation_regex.swift)
    endif()
endif()

option(REGRAM_TEST_DEBUG_GRAPHICS "Build Debug pixel equivalence and blending probes" OFF)
if (REGRAM_TEST_DEBUG_GRAPHICS)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui)
    add_executable(regram_debug_graphics_probe debug_graphics_probe.cpp)
    target_compile_features(regram_debug_graphics_probe PRIVATE cxx_std_20)
    target_link_libraries(regram_debug_graphics_probe PRIVATE Qt6::Core Qt6::Gui)
    if (TARGET Qt6::QCocoaIntegrationPlugin)
        qt_import_plugins(regram_debug_graphics_probe INCLUDE Qt6::QCocoaIntegrationPlugin)
    endif()
endif()

option(REGRAM_TEST_FONTS "Build native font compatibility tests" OFF)
if (REGRAM_TEST_FONTS)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui)
    add_executable(regram_typography_tests typography_tests.cpp ../typography/font_data.cpp)
    target_include_directories(regram_typography_tests PRIVATE ../..)
    if (WIN32)
        target_link_libraries(regram_typography_tests PRIVATE dwrite)
    endif()
    target_compile_features(regram_typography_tests PRIVATE cxx_std_20)
    target_link_libraries(regram_typography_tests PRIVATE Qt6::Core Qt6::Gui)
    target_compile_definitions(regram_typography_tests PRIVATE
        REGRAM_FONT_CATALOG="${CMAKE_CURRENT_SOURCE_DIR}/../../../Resources/regram/fonts/catalog.json")
    if (TARGET Qt6::QCocoaIntegrationPlugin)
        qt_import_plugins(regram_typography_tests INCLUDE Qt6::QCocoaIntegrationPlugin)
    endif()
    add_test(NAME regram_typography_tests COMMAND regram_typography_tests)
endif()
