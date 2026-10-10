set(REGRAM_DEBUG_QT_GUI_LIBRARY "" CACHE FILEPATH
    "Isolated Qt Debug archive prepared by prepare_debug_qt.py")

if (NOT APPLE OR NOT REGRAM_DEBUG_QT_GUI_LIBRARY)
    return()
endif()

get_filename_component(regram_debug_qt_directory
    "${REGRAM_DEBUG_QT_GUI_LIBRARY}" DIRECTORY)
set(regram_debug_qt_manifest "${regram_debug_qt_directory}/manifest.json")
if (NOT EXISTS "${REGRAM_DEBUG_QT_GUI_LIBRARY}"
    OR NOT EXISTS "${regram_debug_qt_manifest}")
    message(FATAL_ERROR "Prepare the isolated Qt Debug archive and manifest first.")
endif()

file(READ "${regram_debug_qt_manifest}" regram_debug_qt_metadata)
string(JSON regram_debug_qt_configuration GET
    "${regram_debug_qt_metadata}" configuration)
string(JSON regram_debug_qt_version GET "${regram_debug_qt_metadata}" qt)
string(JSON regram_debug_qt_symbols GET
    "${regram_debug_qt_metadata}" debug_symbols)
string(JSON regram_debug_qt_assertions GET
    "${regram_debug_qt_metadata}" original_assertions_and_hardening)
if (NOT regram_debug_qt_configuration STREQUAL "Debug"
    OR NOT regram_debug_qt_version VERSION_EQUAL QT_VERSION
    OR NOT regram_debug_qt_symbols
    OR NOT regram_debug_qt_assertions)
    message(FATAL_ERROR "The Qt Debug optimization must match Qt and retain debugging checks.")
endif()

get_target_property(regram_debug_qt_original Qt6::Gui IMPORTED_LOCATION_DEBUG)
file(SHA256 "${regram_debug_qt_original}" regram_debug_qt_original_hash)
file(SHA256 "${REGRAM_DEBUG_QT_GUI_LIBRARY}" regram_debug_qt_archive_hash)
string(JSON regram_debug_qt_expected_original GET
    "${regram_debug_qt_metadata}" original_archive_sha256)
string(JSON regram_debug_qt_expected_archive GET
    "${regram_debug_qt_metadata}" archive_sha256)
if (NOT regram_debug_qt_original_hash STREQUAL regram_debug_qt_expected_original
    OR NOT regram_debug_qt_archive_hash STREQUAL regram_debug_qt_expected_archive)
    message(FATAL_ERROR "The Qt Debug archive changed; rerun prepare_debug_qt.py.")
endif()

set_target_properties(Qt6::Gui PROPERTIES
    IMPORTED_LOCATION_DEBUG "${REGRAM_DEBUG_QT_GUI_LIBRARY}")
get_target_property(regram_debug_qt_links external_qt INTERFACE_LINK_LIBRARIES)
set(regram_debug_qt_fast_links "")
set(regram_debug_qt_replaced 0)
foreach (regram_debug_qt_link IN LISTS regram_debug_qt_links)
    if (regram_debug_qt_link MATCHES "/libQt6Gui.*\\.a$")
        math(EXPR regram_debug_qt_replaced "${regram_debug_qt_replaced} + 1")
        list(APPEND regram_debug_qt_fast_links
            "$<IF:$<CONFIG:Debug>,${REGRAM_DEBUG_QT_GUI_LIBRARY},${regram_debug_qt_link}>")
    else()
        list(APPEND regram_debug_qt_fast_links "${regram_debug_qt_link}")
    endif()
endforeach()
if (NOT DESKTOP_APP_USE_PACKAGED AND NOT regram_debug_qt_replaced)
    message(FATAL_ERROR "The static Qt Gui Debug link must use the prepared archive.")
endif()
set_target_properties(external_qt PROPERTIES
    INTERFACE_LINK_LIBRARIES "${regram_debug_qt_fast_links}")
message(STATUS "Regram: using isolated Qt Debug pixel-loop optimizations.")
