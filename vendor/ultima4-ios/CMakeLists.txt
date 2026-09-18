cmake_minimum_required(VERSION 3.13)
project(zu4 CXX C)

# ---------------------------------------------------------------------------
# iOS port of zu4 (Ultima IV). This CMake file targets iOS only; the desktop
# build still uses the Makefile. Configure with:
#   -DCMAKE_SYSTEM_NAME=iOS -DSDL2_INCLUDE_DIR=... -DSDL2_LIBRARY=... etc.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Force the C++ standard through to the Xcode compiler. CMAKE_CXX_STANDARD alone
# doesn't reliably reach Xcode's per-file flags, and the engine uses C++11
# features (e.g. braced assignment `x = {0,0,0}`) that fail under the default.
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "gnu++14")

# Sources (mirrors CSRCS/CXXSRCS in the Makefile, plus the bundled miniz).
set(ZU4_C_SRCS
    deps/yxml/yxml.c
    deps/miniz/miniz.c
    src/lzw/hash.c src/lzw/lzw.c src/lzw/u4decode.c
    src/armor.c src/cmixer.c src/coords.c src/direction.c src/error.c
    src/experience_settings.c src/image.c src/imageloader.c src/io.c src/moongate.c src/music.c
    src/names.c src/random.c src/rle.c src/savegame.c src/settings.c
    src/sound.c src/stb_vorbis.c src/xmlparse.c src/u4_sdl.c src/video.c
    src/weapon.c src/u4file.c)

set(ZU4_CXX_SRCS
    src/annotation.cpp src/aura.cpp src/camp.cpp src/cheat.cpp src/city.cpp
    src/codex.cpp src/combat.cpp src/config.cpp src/controller.cpp
    src/topicjournal.cpp src/journal_notebook.cpp src/mobile_map_pins.cpp src/mobile_map_discoveries.cpp src/mobile_dungeon_exploration.cpp src/mobile_dungeon_sight.cpp src/context.cpp src/conversation.cpp src/creature.cpp src/death.cpp
    src/dialogueloader.cpp src/dialogueloader_hw.cpp src/dialogueloader_lb.cpp
    src/dialogueloader_tlk.cpp src/dungeon.cpp src/dungeonview.cpp src/event.cpp
    src/event_sdl.cpp src/game.cpp src/imagemgr.cpp src/imageview.cpp
    src/intro.cpp src/item.cpp src/location.cpp src/map.cpp src/maploader.cpp
    src/mapmgr.cpp src/menu.cpp src/menuitem.cpp src/movement.cpp src/object.cpp
    src/person.cpp src/player.cpp src/portal.cpp src/progress_bar.cpp
    src/script.cpp src/screen.cpp src/shrine.cpp src/spell.cpp src/stats.cpp
    src/textview.cpp src/tile.cpp src/tileanim.cpp src/tilemap.cpp
    src/tileset.cpp src/tileview.cpp src/u4.cpp src/view.cpp src/xml.cpp)

set(ZU4_IOS_SRCS ios/zu4_ios_ui.mm ios/topic_panel.mm ios/test_tools_panel.mm ios/game_data_bootstrap.mm ios/adventure_package.mm ios/adventure_transfer.mm ios/cloud_accounts.mm)
set_source_files_properties(ios/adventure_package.mm ios/adventure_transfer.mm ios/cloud_accounts.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
set_source_files_properties(ios/topic_panel.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
set_source_files_properties(ios/test_tools_panel.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
set_source_files_properties(ios/game_data_bootstrap.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")

add_compile_definitions(ZU4_IOS)

include_directories(
    src src/lzw deps/yxml deps/miniz ios
    ${SDL2_INCLUDE_DIR}
    ${CMAKE_OSX_SYSROOT}/usr/include/libxml2)

add_executable(zu4 MACOSX_BUNDLE
    ${ZU4_C_SRCS} ${ZU4_CXX_SRCS} ${ZU4_IOS_SRCS})

option(ZU4_IOS_COMBAT_RUNTIME_TESTS "Isolated simulator combat integration tests" OFF)
if(ZU4_IOS_COMBAT_RUNTIME_TESTS)
    if(NOT CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
        message(FATAL_ERROR "Combat runtime fixtures must never be built for a physical device")
    endif()
    target_sources(zu4 PRIVATE tests/ios_combat_runtime_test.mm)
    target_compile_definitions(zu4 PRIVATE ZU4_IOS_COMBAT_RUNTIME_TESTS)
endif()

# --- iOS frameworks + libs ---
option(ZU4_IOS_AUDIO_RUNTIME_TESTS "Isolated simulator audio integration tests" OFF)
if(ZU4_IOS_AUDIO_RUNTIME_TESTS)
    if(NOT CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
        message(FATAL_ERROR "Audio fixtures must never be built for a physical device")
    endif()
    target_sources(zu4 PRIVATE tests/ios_audio_runtime_test.mm)
    target_compile_definitions(zu4 PRIVATE ZU4_IOS_AUDIO_RUNTIME_TESTS)
endif()
option(ZU4_IOS_JOURNAL_RUNTIME_TESTS "Isolated simulator journal integration tests" OFF)
if(ZU4_IOS_JOURNAL_RUNTIME_TESTS)
    if(NOT CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
        message(FATAL_ERROR "Journal fixtures must never be built for a physical device")
    endif()
    target_sources(zu4 PRIVATE tests/ios_journal_runtime_test.mm)
    target_compile_definitions(zu4 PRIVATE ZU4_IOS_JOURNAL_RUNTIME_TESTS)
endif()

find_library(FW_WEBKIT WebKit)
find_library(FW_SECURITY Security)
find_library(FW_UIKIT UIKit)
option(ZU4_IOS_ADVENTURE_RUNTIME_TESTS "Isolated simulator adventure transfer tests" OFF)
if(ZU4_IOS_ADVENTURE_RUNTIME_TESTS)
    if(NOT CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
        message(FATAL_ERROR "Adventure fixtures must never be built for a physical device")
    endif()
    target_sources(zu4 PRIVATE tests/ios_adventure_runtime_test.mm)
    set_source_files_properties(tests/ios_adventure_runtime_test.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_compile_definitions(zu4 PRIVATE ZU4_IOS_ADVENTURE_RUNTIME_TESTS)
endif()
option(ZU4_IOS_CLOUD_RUNTIME_TESTS "Isolated simulator cloud integration tests" OFF)
if(ZU4_IOS_CLOUD_RUNTIME_TESTS)
    if(NOT CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
        message(FATAL_ERROR "Cloud runtime fixtures must never be built for a physical device")
    endif()
    target_sources(zu4 PRIVATE tests/ios_cloud_runtime_test.mm)
    set_source_files_properties(tests/ios_cloud_runtime_test.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_compile_definitions(zu4 PRIVATE ZU4_IOS_CLOUD_RUNTIME_TESTS)
endif()
find_library(FW_FOUNDATION Foundation)
find_library(FW_COREGRAPHICS CoreGraphics)
find_library(FW_QUARTZCORE QuartzCore)
find_library(FW_COREMOTION CoreMotion)
find_library(FW_METAL Metal)
find_library(FW_OPENGLES OpenGLES)
find_library(FW_AUDIOTOOLBOX AudioToolbox)
find_library(FW_AVFOUNDATION AVFoundation)
find_library(FW_COREAUDIO CoreAudio)
find_library(FW_COREVIDEO CoreVideo)
find_library(FW_GAMECONTROLLER GameController)
find_library(FW_COREHAPTICS CoreHaptics)
find_library(FW_IMAGEIO ImageIO)
find_library(FW_COREFOUNDATION CoreFoundation)
find_library(FW_COREBLUETOOTH CoreBluetooth)

target_link_libraries(zu4
    ${SDL2_LIBRARY} ${SDL2MAIN_LIBRARY}
    "-lxml2" "-lz"
    ${FW_WEBKIT} ${FW_SECURITY} ${FW_UIKIT} ${FW_FOUNDATION} ${FW_COREGRAPHICS} ${FW_QUARTZCORE}
    ${FW_COREMOTION} ${FW_METAL} ${FW_OPENGLES} ${FW_AUDIOTOOLBOX}
    ${FW_AVFOUNDATION} ${FW_COREAUDIO} ${FW_COREVIDEO} ${FW_GAMECONTROLLER}
    ${FW_COREHAPTICS} ${FW_IMAGEIO} ${FW_COREFOUNDATION} ${FW_COREBLUETOOTH})

IF(NOT ZU4_IOS_BUNDLE_ID)
    set(ZU4_IOS_BUNDLE_ID "org.example.ultimatum.u4")
ENDIF()
IF(NOT ZU4_IOS_VERSION)
    set(ZU4_IOS_VERSION "1.0")
ENDIF()
IF(NOT ZU4_IOS_BUILD_NUMBER)
    set(ZU4_IOS_BUILD_NUMBER "1")
ENDIF()
IF(NOT ZU4_CLOUD_SESSION_ACCOUNT)
    set(ZU4_CLOUD_SESSION_ACCOUNT "ultimatum-account-unconfigured.invalid")
ENDIF()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(ios/cloud_config.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/cloud_config.h" @ONLY)
configure_file(ios/ExportOptions-TestFlight.plist.in "${CMAKE_CURRENT_BINARY_DIR}/ExportOptions-TestFlight.plist" @ONLY)
target_include_directories(zu4 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
IF(ZU4_IOS_DEBUG_TOOLS)
    set(ZU4_IOS_DEBUG_TOOLS_PLIST "<true/>")
ELSE()
    set(ZU4_IOS_DEBUG_TOOLS_PLIST "<false/>")
ENDIF()

set_target_properties(zu4 PROPERTIES
    OUTPUT_NAME "UltimatumU4"
    MACOSX_BUNDLE_BUNDLE_NAME "UltimatumU4"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${ZU4_IOS_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${ZU4_IOS_BUILD_NUMBER}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${ZU4_IOS_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Info.plist.in"
    XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
    XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ZU4_IOS_BUNDLE_ID}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "13.0")

# App icon: compile ios/Assets.xcassets (holding the AppIcon set) into the bundle.
target_sources(zu4 PRIVATE "${CMAKE_SOURCE_DIR}/ios/Assets.xcassets")
set_source_files_properties("${CMAKE_SOURCE_DIR}/ios/Assets.xcassets"
    PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_target_properties(zu4 PROPERTIES
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")

IF(ZU4_IOS_TEAM)
    set_target_properties(zu4 PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${ZU4_IOS_TEAM}")
ENDIF()

# Share the authored account UI and pinned auth SDK with the web client. The
# repository default is cloud-disabled; normal iOS scripts point this at the
# ignored, locally configured copy under clients/web/.cache/local-dist.
if(NOT ZU4_ACCOUNT_UI)
    set(ZU4_ACCOUNT_UI "${CMAKE_SOURCE_DIR}/../../clients/web/dist")
endif()
add_custom_command(TARGET zu4 POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:zu4>/accounts")
foreach(file cloud.html cloud.css cloud-sdk.js cloud-sdk.js.LEGAL.txt cloud-config.js cloud-client.js cloud-ui.js cloud-native.js save-store.js game-data.js game-data-manifest.js)
    add_custom_command(TARGET zu4 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ZU4_ACCOUNT_UI}/${file}" "$<TARGET_BUNDLE_DIR:zu4>/accounts/${file}")
endforeach()

# --- Bundle zu4's own data into the app ---
foreach(dir conf graphics sound)
    add_custom_command(TARGET zu4 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/${dir}" "$<TARGET_BUNDLE_DIR:zu4>/${dir}")
endforeach()
# Always replace stale preview audio with the single soundtrack supplied by xu4.
add_custom_command(TARGET zu4 POST_BUILD COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_BUNDLE_DIR:zu4>/music")
add_custom_command(TARGET zu4 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/music/hurin" "$<TARGET_BUNDLE_DIR:zu4>/music/hurin")

# --- Optionally bundle user-supplied Ultima IV files for local development. ---
IF(ZU4_U4_GAMEDIR)
    add_custom_command(TARGET zu4 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ZU4_U4_GAMEDIR}" "$<TARGET_BUNDLE_DIR:zu4>/ultima4")
ELSE()
    message(WARNING "ZU4_U4_GAMEDIR not set - app will build without U4 game data.")
ENDIF()

# Optional graphics-only package produced by ios/prepare-vga-upgrade.sh. The
# engine discovers this exact filename at the bundle root.
IF(ZU4_U4_UPGRADE)
    add_custom_command(TARGET zu4 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ZU4_U4_UPGRADE}" "$<TARGET_BUNDLE_DIR:zu4>/u4upgrad.zip")
ENDIF()

# Strip extended attributes (e.g. com.apple.quarantine on downloaded game data)
# from the whole bundle. codesign rejects "resource fork / Finder information"
# detritus, so this must run before the (later) CodeSign build phase.
add_custom_command(TARGET zu4 POST_BUILD
    COMMAND xattr -cr "$<TARGET_BUNDLE_DIR:zu4>")
