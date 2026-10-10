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
