# Nagram Desktop: api_id and api_hash come from the environment
# (NAGRAM_API_ID, NAGRAM_API_HASH) or from the uncommitted file
# Telegram/build/api_credentials.local.cmake. Both are ignored with
# -D TDESKTOP_API_TEST=ON; explicit -D TDESKTOP_API_ID / TDESKTOP_API_HASH
# values are used only while neither source is set.

set(nagram_api_local ${CMAKE_CURRENT_SOURCE_DIR}/build/api_credentials.local.cmake)
if (EXISTS ${nagram_api_local})
    include(${nagram_api_local})
endif()
if (DEFINED ENV{NAGRAM_API_ID} AND NOT "$ENV{NAGRAM_API_ID}" STREQUAL "")
    set(NAGRAM_API_ID "$ENV{NAGRAM_API_ID}")
endif()
if (DEFINED ENV{NAGRAM_API_HASH} AND NOT "$ENV{NAGRAM_API_HASH}" STREQUAL "")
    set(NAGRAM_API_HASH "$ENV{NAGRAM_API_HASH}")
endif()

if (NOT TDESKTOP_API_TEST
    AND (NOT TDESKTOP_API_ID OR TDESKTOP_API_ID STREQUAL "0" OR NAGRAM_API_FROM_LOCAL)
    AND DEFINED NAGRAM_API_ID)
    if (NOT NAGRAM_API_ID MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "NAGRAM_API_ID must be a positive integer.")
    endif()
    string(LENGTH "${NAGRAM_API_HASH}" nagram_api_hash_length)
    if (NOT NAGRAM_API_HASH MATCHES "^[0-9a-f]+$"
        OR NOT nagram_api_hash_length EQUAL 32)
        message(FATAL_ERROR "NAGRAM_API_HASH must be 32 lowercase hex digits.")
    endif()
    set(TDESKTOP_API_ID ${NAGRAM_API_ID} CACHE STRING "Provide 'api_id' for the Telegram API access." FORCE)
    set(TDESKTOP_API_HASH ${NAGRAM_API_HASH} CACHE STRING "Provide 'api_hash' for the Telegram API access." FORCE)
    set(NAGRAM_API_FROM_LOCAL ON CACHE INTERNAL "api_id and api_hash were taken from NAGRAM_API_*.")
    message(STATUS "Nagram: using api_id ${NAGRAM_API_ID} from the environment or local credentials file.")
endif()
