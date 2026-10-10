set(nagram_sources
    regram/ayu/features/translator/cache_key.cpp
    regram/nagram/services/translation_model.cpp
    regram/nagram/services/send_translation_model.cpp
    regram/nagram/services/send_translation.cpp
    regram/nagram/privacy/registration_model.cpp
    regram/nagram/notifications/settings.cpp
    regram/nagram/notifications/review.cpp
    regram/nagram/notifications/model.cpp
    regram/nagram/messages/markdown.cpp
    regram/nagram/compose/forward.cpp
    regram/nagram/chats/cleanup_model.cpp
    regram/nagram/chats/cleanup.cpp
    regram/nagram/chats/layout.cpp
    regram/nagram/chats/list_refresher.cpp
    regram/nagram/chats/promotions.cpp
    regram/nagram/compose/channel.cpp
    regram/nagram/compose/confirm.cpp
    regram/nagram/compose/format_toolbar.cpp
    regram/nagram/compose/placeholder.cpp
    regram/nagram/compose/spacing.cpp
    regram/nagram/compose/text.cpp
    regram/nagram/core/language.cpp
    regram/nagram/core/exchange.cpp
    regram/nagram/core/options.cpp
    regram/nagram/core/regex.cpp
    regram/nagram/core/diagnostics.cpp
    regram/nagram/core/relaunch.cpp
    regram/nagram/display/view_refresher.cpp
    regram/nagram/export/range_model.cpp
    regram/nagram/interface/main_menu.cpp
    regram/nagram/interface/main_menu_model.cpp
    regram/nagram/interface/notifications.cpp
    regram/nagram/interface/text.cpp
    regram/nagram/interface/appearance.cpp
    regram/nagram/interface/app_icon.cpp
    regram/nagram/chats/startup_folder.cpp
    regram/nagram/chats/sort.cpp
    regram/nagram/chats/recent_chats.cpp
    regram/nagram/chats/local_pins.cpp
    regram/nagram/chats/local_pins_model.cpp
    regram/nagram/chats/reading_position.cpp
    regram/nagram/chats/tools.cpp
    regram/nagram/chats/community.cpp
    regram/nagram/chats/folders.cpp
    regram/nagram/chats/managed_folders.cpp
    regram/nagram/interface/roundness.cpp
    regram/nagram/settings/interface.cpp
    regram/nagram/settings/restart.cpp
    regram/nagram/menu/actions.cpp
    regram/nagram/menu/batch.cpp
    regram/nagram/menu/media.cpp
    regram/nagram/menu/download.cpp
    regram/nagram/menu/draft.cpp
    regram/nagram/menu/rating.cpp
    regram/nagram/menu/message_tools.cpp
    regram/nagram/menu/model.cpp
    regram/nagram/menu/repeat.cpp
    regram/nagram/menu/reading.cpp
    regram/nagram/menu/selection.cpp
	regram/nagram/media/sticker_catalog.cpp
    regram/nagram/media/audio.cpp
    regram/nagram/media/cover.cpp
    regram/nagram/media/sticker_export.cpp
    regram/nagram/media/sticker_export_model.cpp
    regram/nagram/media/url_template.cpp
    regram/nagram/media/extras.cpp
    regram/nagram/media/local_faved.cpp
    regram/nagram/media/local_faved_model.cpp
    regram/nagram/messages/format.cpp
    regram/nagram/messages/content.cpp
    regram/nagram/messages/badges.cpp
    regram/nagram/messages/effects.cpp
    regram/nagram/messages/online.cpp
    regram/nagram/messages/reactions.cpp
    regram/nagram/messages/reading.cpp
    test/nagram_menu_scenario.cpp
    regram/nagram/filters/model.cpp
    regram/nagram/filters/view.cpp
    regram/nagram/filters/settings.cpp
    regram/nagram/filters/menu.cpp
    regram/nagram/filters/hidden_messages.cpp
    regram/nagram/links/model.cpp
    regram/nagram/links/open.cpp
    regram/nagram/links/behavior.cpp
    regram/nagram/links/inline_bot.cpp
    regram/nagram/links/inline_rules.cpp
    regram/nagram/links/inline_settings.cpp
    regram/nagram/links/webview.cpp
    regram/nagram/links/settings.cpp
    regram/nagram/network/model.cpp
    regram/nagram/network/runtime.cpp
    regram/nagram/snapshot/snapshot.cpp
    regram/nagram/snapshot/cloud_theme.cpp
    regram/nagram/snapshot/cloud_theme_model.cpp
    regram/nagram/privacy/profile.cpp
    regram/nagram/privacy/alias.cpp
    regram/nagram/privacy/admin_shortcuts.cpp
    regram/nagram/privacy/display.cpp
    regram/nagram/privacy/alias_model.cpp
    regram/nagram/privacy/protection.cpp
    regram/nagram/privacy/protection_model.cpp
    regram/nagram/settings/home.cpp
    regram/nagram/settings/rules.cpp
    regram/nagram/settings/network.cpp
    regram/nagram/settings/chats.cpp
    regram/nagram/settings/compose.cpp
	regram/nagram/settings/config.cpp
    regram/nagram/settings/cloud_sync.cpp
    regram/nagram/sync/model.cpp
    regram/nagram/sync/saved_messages.cpp
    regram/nagram/sync/service.cpp
    regram/nagram/settings/media.cpp
    regram/nagram/settings/menu.cpp
    regram/nagram/settings/privacy.cpp
    regram/nagram/settings/messages.cpp
    regram/nagram/settings/services.cpp
    regram/nagram/services/credentials.cpp
    regram/nagram/services/auto_translate.cpp
    regram/nagram/services/auto_translate_model.cpp
    regram/nagram/services/chat_translation.cpp
    regram/nagram/services/chat_translation_model.cpp
    regram/nagram/services/context.cpp
    regram/nagram/services/context_model.cpp
    regram/nagram/services/model.cpp
    regram/nagram/services/presets.cpp
    regram/nagram/services/store.cpp
    regram/nagram/services/summary.cpp
    regram/nagram/services/summary_model.cpp
    regram/nagram/services/request.cpp
    regram/nagram/services/translation.cpp
    regram/nagram/services/draft_translation.cpp
    regram/nagram/services/transcription.cpp
    regram/nagram/services/transcription_queue.cpp
    regram/nagram/services/system_ai.cpp
)

if (nagram_sources)
    nice_target_sources(Telegram ${src_loc} PRIVATE ${nagram_sources})
endif()

if (APPLE AND NOT DESKTOP_APP_DISABLE_SWIFT6)
    enable_language(Swift)
    set_target_properties(Telegram PROPERTIES LINKER_LANGUAGE CXX)
    set(nagram_swift_deployment "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if (NOT nagram_swift_deployment OR nagram_swift_deployment VERSION_LESS 11.0)
        set(nagram_swift_deployment 11.0)
    endif()
    if (NOT CMAKE_GENERATOR STREQUAL "Xcode")
        set(nagram_swift_arch "${CMAKE_OSX_ARCHITECTURES}")
        if (NOT nagram_swift_arch)
            set(nagram_swift_arch "${CMAKE_SYSTEM_PROCESSOR}")
        endif()
        list(LENGTH nagram_swift_arch nagram_swift_arch_count)
        if (NOT nagram_swift_arch_count EQUAL 1)
            message(FATAL_ERROR "Use Xcode for a universal Swift build, or select one CMAKE_OSX_ARCHITECTURES value.")
        endif()
    endif()
    function(nagram_configure_swift_target target_name)
        if (CMAKE_GENERATOR STREQUAL "Xcode")
            set_target_properties(${target_name} PROPERTIES
                XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "${nagram_swift_deployment}")
        else()
            target_compile_options(${target_name} PRIVATE
                "-target" "${nagram_swift_arch}-apple-macos${nagram_swift_deployment}")
        endif()
    endfunction()
    nagram_configure_swift_target(lib_translate)
    find_library(NAGRAM_FOUNDATION_MODELS FoundationModels)
    if (NAGRAM_FOUNDATION_MODELS)
        add_library(nagram_system_ai STATIC
            ${src_loc}/regram/nagram/services/system_ai.swift)
        set_target_properties(nagram_system_ai PROPERTIES
            Swift_LANGUAGE_VERSION 6
            Swift_COMPILATION_MODE wholemodule)
        nagram_configure_swift_target(nagram_system_ai)
        target_link_options(nagram_system_ai INTERFACE
            "SHELL:-weak_framework FoundationModels")
        target_link_libraries(Telegram PRIVATE nagram_system_ai)
        target_compile_definitions(Telegram PRIVATE NAGRAM_SYSTEM_AI)
    endif()
endif()

nice_target_sources(Telegram ${res_loc} PRIVATE qrc/nagram.qrc)
if (APPLE)
    nice_target_sources(Telegram ${res_loc} PRIVATE qrc/nagram_mac.qrc)
endif()

# Compiles the Icon Composer document so macOS renders the app icon itself.
function(nagram_mac_app_icon result)
    set(${result} FALSE PARENT_SCOPE)
    execute_process(
        COMMAND xcodebuild -version
        OUTPUT_VARIABLE xcode_version_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if (NOT xcode_version_output MATCHES "Xcode ([0-9]+)")
        message(STATUS "Nagram: Xcode not found, using the static app icon.")
        return()
    elseif (CMAKE_MATCH_1 LESS 26)
        message(STATUS "Nagram: Xcode ${CMAKE_MATCH_1} cannot compile .icon documents, using the static app icon.")
        return()
    endif()
    set(icon_source ${res_loc}/branding/Regram.icon)
    set(icon_output ${CMAKE_CURRENT_BINARY_DIR}/nagram_icon)
    set(icon_deployment "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if (NOT icon_deployment)
        set(icon_deployment 11.0)
    endif()
    set(icon_sources ${icon_source})
    file(GLOB_RECURSE icon_inputs CONFIGURE_DEPENDS ${icon_source}/*)
    add_custom_command(
        OUTPUT ${icon_output}/Assets.car ${icon_output}/Regram.icns
        COMMAND ${CMAKE_COMMAND} -E make_directory ${icon_output}
        COMMAND xcrun actool ${icon_sources}
            --compile ${icon_output}
            --app-icon Regram
            --platform macosx
            --minimum-deployment-target ${icon_deployment}
            --output-partial-info-plist ${icon_output}/partial.plist
            --output-format human-readable-text
        DEPENDS ${icon_inputs}
        VERBATIM)
    set_source_files_properties(
        ${icon_output}/Assets.car
        ${icon_output}/Regram.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_add_resource(Telegram
        ${icon_output}/Assets.car
        ${icon_output}/Regram.icns)
    set(${result} TRUE PARENT_SCOPE)
endfunction()

if (DESKTOP_APP_TEST_APPS)
    add_executable(test_nagram)
    init_target(test_nagram "(tests)")
    target_precompile_headers(test_nagram PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib_base/base/basic_types.h)

    nice_target_sources(test_nagram ${src_loc} PRIVATE
        test/test_model_ui_runtime.cpp
        regram/ayu/features/translator/cache_key.cpp
        regram/nagram/services/translation_model.cpp
        regram/settings/advanced_model.cpp
        regram/nagram/tests/test_advanced_settings.cpp
        regram/nagram/tests/test_translation.cpp
        regram/nagram/tests/test_performance.cpp
        regram/nagram/privacy/registration_model.cpp
        regram/nagram/messages/markdown.cpp
        regram/nagram/services/send_translation_model.cpp
        regram/nagram/notifications/model.cpp
        regram/nagram/chats/cleanup_model.cpp
        regram/nagram/tests/test_send_translation.cpp
        regram/nagram/tests/test_notifications.cpp
        regram/nagram/tests/test_cleanup.cpp
        regram/nagram/tests/test_markdown.cpp
        regram/nagram/tests/test_lang.cpp
        regram/nagram/tests/test_options.cpp
        regram/nagram/tests/test_spacing.cpp
        regram/nagram/tests/test_services.cpp
        regram/nagram/tests/test_chat_translation.cpp
        regram/nagram/tests/test_auto_translate.cpp
        regram/nagram/tests/test_filters.cpp
        regram/nagram/tests/test_filter_scopes.cpp
        regram/nagram/tests/test_links.cpp
        regram/nagram/tests/test_inline_rules.cpp
        regram/nagram/tests/test_privacy.cpp
        regram/nagram/tests/test_p3_misc.cpp
        regram/nagram/tests/test_local_lists.cpp
        regram/nagram/tests/test_network.cpp
        regram/nagram/tests/test_media.cpp
        regram/nagram/tests/test_sync.cpp
        regram/nagram/tests/test_export.cpp
        regram/nagram/compose/spacing.cpp
        regram/nagram/compose/default_format.cpp
        regram/nagram/compose/chat_preferences.cpp
        regram/nagram/core/exchange.cpp
        regram/nagram/core/regex.cpp
        regram/nagram/export/range_model.cpp
        regram/nagram/interface/main_menu_model.cpp
        regram/nagram/menu/model.cpp
        regram/nagram/services/auto_translate_model.cpp
        regram/nagram/services/chat_translation_model.cpp
        regram/nagram/services/context_model.cpp
        regram/nagram/services/model.cpp
        regram/nagram/services/presets.cpp
        regram/nagram/services/summary_model.cpp
        regram/nagram/services/transcription_queue.cpp
        regram/nagram/filters/model.cpp
        regram/nagram/links/model.cpp
        regram/nagram/links/inline_rules.cpp
        regram/nagram/links/webview.cpp
        regram/nagram/privacy/protection_model.cpp
        regram/nagram/chats/local_pins_model.cpp
        regram/nagram/media/local_faved_model.cpp
        regram/nagram/network/model.cpp
        regram/nagram/media/url_template.cpp
        regram/nagram/media/sticker_export_model.cpp
        regram/nagram/snapshot/cloud_theme_model.cpp
        regram/nagram/sync/model.cpp
    )

    target_include_directories(test_nagram PRIVATE
        ${src_loc}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib_ui)

    target_link_libraries(test_nagram PRIVATE
        desktop-app::lib_base
        desktop-app::lib_crl
        desktop-app::lib_ui
        desktop-app::external_qt
    )

    target_compile_definitions(test_nagram PRIVATE
        NAGRAM_LANG_SOURCE_DIR="${res_loc}/langs"
    )

    set_target_properties(test_nagram PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/nagram-tests/$<CONFIG>"
    )

    add_dependencies(Telegram test_nagram)
endif()
