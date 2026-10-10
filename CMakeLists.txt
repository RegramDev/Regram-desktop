# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.25...3.31)

if (CMAKE_HOST_APPLE AND
    (NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 12.0))
    set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "Minimum macOS deployment version" FORCE)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/validate_special_target.cmake)
set(REGRAM_DEPENDENCIES_DIR "" CACHE PATH "Isolated Regram build dependencies")
if (REGRAM_DEPENDENCIES_DIR)
    if (NOT IS_DIRECTORY "${REGRAM_DEPENDENCIES_DIR}/local")
        message(FATAL_ERROR "The Regram dependency directory needs a local prefix.")
    endif()
    get_filename_component(libs_loc "${REGRAM_DEPENDENCIES_DIR}" REALPATH)
endif()
include(cmake/version.cmake)
desktop_app_parse_version(Telegram/build/version)

if (NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(configuration_types_init 1)
endif()

project(Telegram
    LANGUAGES C CXX
    VERSION ${desktop_app_version_cmake}
    DESCRIPTION "Regram Desktop Telegram client"
    HOMEPAGE_URL "https://github.com/RegramDev/Regram-desktop"
)

if (APPLE)
    enable_language(OBJC OBJCXX)
endif()

if (configuration_types_init
    AND CMAKE_CONFIGURATION_TYPES
    AND NOT MinSizeRel IN_LIST CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES};MinSizeRel" CACHE STRING "" FORCE)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Telegram)

get_filename_component(third_party_loc "Telegram/ThirdParty" REALPATH)
get_filename_component(submodules_loc "Telegram" REALPATH)
get_filename_component(cmake_helpers_loc "cmake" REALPATH)

if (NOT DESKTOP_APP_USE_PACKAGED AND WIN32)
    set(Python3_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/python/Scripts/python)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/Telegram/build/regram/apply_patches.py
    RESULT_VARIABLE regram_patches_result
    OUTPUT_VARIABLE regram_patches_output
    ERROR_VARIABLE regram_patches_error
)
if (NOT regram_patches_result EQUAL 0)
    message(FATAL_ERROR "${regram_patches_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    Telegram/build/regram/apply_patches.py
    Telegram/build/regram/patches/manifest.json
    Telegram/build/regram/patches/codegen.patch
    Telegram/build/regram/patches/lib_ui.patch
    Telegram/build/regram/patches/lib_tl.patch
)

set(DESKTOP_APP_DISABLE_AUTOUPDATE ON CACHE BOOL "Use manual Regram updates" FORCE)
set(DESKTOP_APP_DISABLE_CRASH_REPORTS ON CACHE BOOL "Disable upstream crash submission" FORCE)

if (DESKTOP_APP_SPECIAL_TARGET MATCHES "^(macstore|uwp|uwp64|uwparm)$")
    message(FATAL_ERROR "Regram store distribution is not configured.")
endif()

include(cmake/variables.cmake)
include(cmake/nice_target_sources.cmake)
include(cmake/target_compile_options_if_exists.cmake)
include(cmake/target_link_frameworks.cmake)
include(cmake/target_link_options_if_exists.cmake)
include(cmake/init_target.cmake)
include(cmake/generate_target.cmake)
include(cmake/nuget.cmake)
include(cmake/validate_d3d_compiler.cmake)
include(cmake/target_prepare_qrc.cmake)

include(cmake/options.cmake)
include(cmake/external/qt/package.cmake)

set(desktop_app_skip_libs
    glibmm
    variant
)

add_subdirectory(cmake)
if (APPLE AND NOT DESKTOP_APP_USE_PACKAGED
    AND EXISTS "${libs_loc}/local/lib/libde265.a")
    target_link_libraries(external_qt_static_plugins_kimageformats
        PUBLIC "${libs_loc}/local/lib/libde265.a")
endif()
add_subdirectory(Telegram)
