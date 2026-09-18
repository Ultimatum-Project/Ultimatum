cmake_minimum_required(VERSION 3.20)
project(ultimatum_web_engine C CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Configure this target through emcmake.")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(ENGINE_ROOT "${REPOSITORY_ROOT}/vendor/ultima4-ios")
set(WEB_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/.cache/prefix")
set(ULTIMATUM_U4_DATA "${CMAKE_CURRENT_SOURCE_DIR}/.cache/game-data" CACHE PATH
    "Directory containing the user's Ultima IV DOS data")
set(ULTIMATUM_U4_UPGRADE "${CMAKE_CURRENT_SOURCE_DIR}/.cache/u4upgrad.zip" CACHE FILEPATH
    "Optional Ultima IV VGA upgrade overlay")
option(ULTIMATUM_BUNDLE_U4_DATA "Bundle local Ultima IV data for development" ON)
option(ULTIMATUM_WEB_DIAGNOSTICS "Build the browser engine with runtime diagnostics" OFF)
option(ULTIMATUM_WEB_RUNTIME_TESTS "Enable isolated development fixture hooks" OFF)
option(ULTIMATUM_WEB_DEBUG_TOOLS "Enable developer-only adventure-changing tools" OFF)
option(ULTIMATUM_BUNDLE_MUSIC "Bundle the soundtrack supplied with xu4" OFF)
set(ULTIMATUM_WEB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist/engine" CACHE PATH "Engine artifact directory")

set(ZU4_C_SOURCES
    deps/yxml/yxml.c
    deps/miniz/miniz.c
    src/lzw/hash.c src/lzw/lzw.c src/lzw/u4decode.c
    src/armor.c src/cmixer.c src/coords.c src/direction.c src/error.c
    src/experience_settings.c src/image.c src/imageloader.c src/io.c
    src/moongate.c src/music.c src/names.c src/random.c src/rle.c
    src/savegame.c src/settings.c src/sound.c src/stb_vorbis.c
    src/xmlparse.c src/u4_sdl.c src/video.c src/weapon.c src/u4file.c)

set(ZU4_CXX_SOURCES
    src/annotation.cpp src/aura.cpp src/camp.cpp src/cheat.cpp src/city.cpp
    src/codex.cpp src/combat.cpp src/config.cpp src/controller.cpp
    src/context.cpp src/conversation.cpp src/creature.cpp src/death.cpp
    src/dialogueloader.cpp src/dialogueloader_hw.cpp src/dialogueloader_lb.cpp
    src/dialogueloader_tlk.cpp src/dungeon.cpp src/dungeonview.cpp
    src/event.cpp src/event_sdl.cpp src/game.cpp src/imagemgr.cpp
    src/imageview.cpp src/intro.cpp src/item.cpp src/location.cpp src/map.cpp
    src/maploader.cpp src/mapmgr.cpp src/menu.cpp src/menuitem.cpp
    src/movement.cpp src/object.cpp src/person.cpp src/player.cpp
    src/portal.cpp src/progress_bar.cpp src/script.cpp src/screen.cpp
    src/shrine.cpp src/spell.cpp src/stats.cpp src/textview.cpp src/tile.cpp
    src/tileanim.cpp src/tilemap.cpp src/tileset.cpp src/tileview.cpp
    src/u4.cpp src/view.cpp src/xml.cpp src/topicjournal.cpp src/journal_notebook.cpp
    src/mobile_map_pins.cpp src/mobile_map_discoveries.cpp
    src/mobile_dungeon_exploration.cpp)

list(TRANSFORM ZU4_C_SOURCES PREPEND "${ENGINE_ROOT}/")
list(TRANSFORM ZU4_CXX_SOURCES PREPEND "${ENGINE_ROOT}/")

add_executable(ultimatum-engine
    ${ZU4_C_SOURCES}
    ${ZU4_CXX_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/web_bridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/web_data_archive.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/web_menu.cpp")
target_sources(ultimatum-engine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/web_journal.cpp")
target_compile_definitions(ultimatum-engine PRIVATE ZU4_WEB)
if(ULTIMATUM_WEB_DEBUG_TOOLS)
    target_compile_definitions(ultimatum-engine PRIVATE ZU4_WEB_DEBUG_TOOLS)
endif()
if(ULTIMATUM_WEB_RUNTIME_TESTS)
    target_compile_definitions(ultimatum-engine PRIVATE ZU4_WEB_RUNTIME_TESTS)
    target_sources(ultimatum-engine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime_fixture.cpp")
endif()
target_include_directories(ultimatum-engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/toolchain"
    "${ENGINE_ROOT}/src"
    "${ENGINE_ROOT}/ios"
    "${ENGINE_ROOT}/src/lzw"
    "${ENGINE_ROOT}/deps/yxml"
    "${ENGINE_ROOT}/deps/miniz"
    "${WEB_PREFIX}/include/libxml2")
target_link_libraries(ultimatum-engine PRIVATE "${WEB_PREFIX}/lib/libxml2.a")
set_property(TARGET ultimatum-engine APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/web_module_config.js")

target_compile_options(ultimatum-engine PRIVATE -O2 "SHELL:-sUSE_SDL=2")
target_link_options(ultimatum-engine PRIVATE
    -O2
    "SHELL:-sUSE_SDL=2"
    "SHELL:-sUSE_ZLIB=1"
    "SHELL:-sASYNCIFY=1"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sFORCE_FILESYSTEM=1"
    "SHELL:-lidbfs.js"
    "SHELL:-sENVIRONMENT=web"
    "SHELL:-sEXIT_RUNTIME=0"
    "SHELL:-sEXPORTED_FUNCTIONS=['_main','_malloc','_free']"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','callMain','FS','IDBFS','HEAPU8']"
    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/toolchain/web_module_config.js"
    "--preload-file=${ENGINE_ROOT}/conf@/conf"
    "--preload-file=${ENGINE_ROOT}/graphics@/graphics"
    "--preload-file=${ENGINE_ROOT}/sound@/sound")
if(ULTIMATUM_BUNDLE_MUSIC)
    if(NOT EXISTS "${ENGINE_ROOT}/music/hurin/wanderer.ogg")
        message(FATAL_ERROR "The xu4 soundtrack is incomplete")
    endif()
    target_link_options(ultimatum-engine PRIVATE
        "--preload-file=${ENGINE_ROOT}/music/hurin@/music/hurin")
    file(GLOB_RECURSE ULTIMATUM_MUSIC_FILES CONFIGURE_DEPENDS
        "${ENGINE_ROOT}/music/hurin/*")
    set_property(TARGET ultimatum-engine APPEND PROPERTY LINK_DEPENDS
        ${ULTIMATUM_MUSIC_FILES})
endif()

if(ULTIMATUM_WEB_DIAGNOSTICS)
    target_compile_options(ultimatum-engine PRIVATE -g2)
    target_link_options(ultimatum-engine PRIVATE
        -g2
        "SHELL:-sASSERTIONS=2"
        "SHELL:-sSTACK_OVERFLOW_CHECK=2")
endif()

if(ULTIMATUM_BUNDLE_U4_DATA
   AND (EXISTS "${ULTIMATUM_U4_DATA}/AVATAR.EXE" OR EXISTS "${ULTIMATUM_U4_DATA}/avatar.exe")
   AND (EXISTS "${ULTIMATUM_U4_DATA}/TITLE.EXE" OR EXISTS "${ULTIMATUM_U4_DATA}/title.exe"))
    message(STATUS "Bundling Ultima IV data from ${ULTIMATUM_U4_DATA}")
    target_link_options(ultimatum-engine PRIVATE
        "--preload-file=${ULTIMATUM_U4_DATA}@/ultima4")

    foreach(SAVE_FILE PARTY.SAV MONSTERS.SAV OUTMONST.SAV DNGMAP.SAV)
        if(EXISTS "${ULTIMATUM_U4_DATA}/${SAVE_FILE}")
            string(TOLOWER "${SAVE_FILE}" SAVE_FILE_LOWER)
            target_link_options(ultimatum-engine PRIVATE
                "--preload-file=${ULTIMATUM_U4_DATA}/${SAVE_FILE}@/home/web_user/.xu4/${SAVE_FILE_LOWER}")
        endif()
    endforeach()
else()
    message(WARNING
        "No development game data preload; the browser importer will request a compatible data set")
endif()

if(EXISTS "${ULTIMATUM_U4_UPGRADE}")
    message(STATUS "Bundling optional VGA overlay from ${ULTIMATUM_U4_UPGRADE}")
    target_link_options(ultimatum-engine PRIVATE
        "--preload-file=${ULTIMATUM_U4_UPGRADE}@/u4upgrad.zip")
    set_property(TARGET ultimatum-engine APPEND PROPERTY LINK_DEPENDS
        "${ULTIMATUM_U4_UPGRADE}")
endif()

set_target_properties(ultimatum-engine PROPERTIES
    OUTPUT_NAME "ultimatum-engine"
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${ULTIMATUM_WEB_OUTPUT_DIRECTORY}")
