nice_target_sources(Telegram ${src_loc}
PRIVATE
    regram/archive/snapshot.cpp
    regram/archive/snapshot.h
    regram/appearance/stories.cpp
    regram/appearance/stories.h
    regram/core/peer_address.cpp
    regram/core/peer_address.h
    regram/core/local_premium.cpp
    regram/core/local_premium.h
    regram/filters/rules.cpp
    regram/filters/rules.h
    regram/filters/matcher.cpp
    regram/filters/matcher.h
    regram/filters/preview.cpp
    regram/filters/preview.h
    regram/filters/preview_policy.cpp
    regram/filters/preview_policy.h
    regram/filters/reveal.cpp
    regram/filters/reveal.h
    regram/filters/reveal_policy.cpp
    regram/filters/reveal_policy.h
    regram/filters/result_cache.cpp
    regram/filters/result_cache.h
    regram/filters/exchange.cpp
    regram/filters/exchange.h
    regram/filters/runtime.cpp
    regram/filters/runtime.h
    regram/filters/search_text.cpp
    regram/filters/search_text.h
    regram/filters/scopes.cpp
    regram/filters/scopes.h
    regram/filters/settings.cpp
    regram/filters/settings.h
    regram/settings/home.cpp
    regram/settings/home.h
    regram/settings/advanced_model.cpp
    regram/settings/advanced_model.h
    regram/settings/shared.cpp
    regram/settings/shared.h
    regram/typography/font_data.cpp
    regram/typography/font_data.h
    regram/typography/fonts.cpp
    regram/typography/fonts.h
    regram/nagram/compose/default_format.cpp
    regram/nagram/compose/default_format.h
    regram/nagram/compose/chat_preferences.cpp
    regram/nagram/compose/chat_preferences.h
    test/test_regram_archive.cpp
    test/test_regram_archive.h
    test/test_regram_integration.cpp
    test/test_regram_integration.h
)

set_source_files_properties(${src_loc}/regram/ayu/libs/sqlite/sqlite3.c PROPERTIES
    COMPILE_DEFINITIONS "SQLITE_HAS_CODEC;SQLITE_TEMP_STORE=2;SQLITE_THREADSAFE=1;SQLITE_EXTRA_INIT=sqlcipher_extra_init;SQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown"
)
target_compile_definitions(Telegram PRIVATE SQLITE_HAS_CODEC)

if (APPLE)
    nice_target_sources(Telegram ${src_loc} PRIVATE
        test/test_regram_mac.mm
    )
endif()

include(${CMAKE_CURRENT_LIST_DIR}/regram_debug_qt.cmake)

if (WIN32)
    target_link_libraries(Telegram PRIVATE dwrite)
endif()

add_executable(test_regram_typography EXCLUDE_FROM_ALL
    ${src_loc}/regram/tests/typography_tests.cpp
    ${src_loc}/regram/typography/font_data.cpp)
init_target(test_regram_typography "(tests)")
set_target_properties(test_regram_typography PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
target_include_directories(test_regram_typography PRIVATE ${src_loc})
target_link_libraries(test_regram_typography PRIVATE desktop-app::external_qt)
target_compile_definitions(test_regram_typography PRIVATE
    REGRAM_FONT_CATALOG="${res_loc}/regram/fonts/catalog.json")
if (WIN32)
    target_link_libraries(test_regram_typography PRIVATE dwrite)
endif()
