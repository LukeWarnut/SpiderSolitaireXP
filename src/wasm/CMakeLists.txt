cmake_minimum_required(VERSION 3.20)
project(spider_wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(WASM_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(REPO_ROOT "${WASM_ROOT}/../.." ABSOLUTE)
set(SPIDER_EXE "${REPO_ROOT}/orig/spider.exe" CACHE FILEPATH "Original spider.exe to take bitmaps and sounds from")
set(ASSET_DIR ${CMAKE_BINARY_DIR}/assets)
set(ASSET_STAMP ${ASSET_DIR}/.extracted)

# Card bitmaps and sounds are copyrighted: extract them from the user's
# spider.exe at build time instead of committing them.
add_custom_command(
    OUTPUT ${ASSET_STAMP}
    COMMAND ${Python3_EXECUTABLE} ${REPO_ROOT}/tools/extract_assets.py ${SPIDER_EXE} ${ASSET_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${ASSET_STAMP}
    DEPENDS ${REPO_ROOT}/tools/extract_assets.py ${REPO_ROOT}/tools/pe_rsrc.py ${SPIDER_EXE}
    COMMENT "Extracting Spider bitmaps and sounds from ${SPIDER_EXE}"
    VERBATIM
)
add_custom_target(spider_assets DEPENDS ${ASSET_STAMP})

# The game core and BMP decoder are shared with the macOS port.
add_executable(spider
    ${WASM_ROOT}/host/main.cpp
    ${WASM_ROOT}/host/wasm_host.cpp
    ${REPO_ROOT}/src/mac/game/game.cpp
    ${REPO_ROOT}/src/mac/render/bmp.cpp
)
add_dependencies(spider spider_assets)
target_include_directories(spider PRIVATE
    ${REPO_ROOT}/src/mac/game
    ${REPO_ROOT}/src/mac/host
    ${REPO_ROOT}/src/mac/render
    ${WASM_ROOT}/host
)
target_compile_options(spider PRIVATE -Wall -Wextra)

# ASYNCIFY: the HTML dialogs are Promises the game awaits. MODULARIZE so
# main.js boots it as SpiderModule().
target_link_options(spider PRIVATE
    --bind
    -sASYNCIFY
    -sASYNCIFY_STACK_SIZE=16384
    -sMODULARIZE=1
    -sEXPORT_NAME=SpiderModule
    -sALLOW_MEMORY_GROWTH=1
    -sENVIRONMENT=web
    -sFORCE_FILESYSTEM=1
    -lidbfs.js
    --no-entry
    -sEXPORTED_RUNTIME_METHODS=FS
)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(spider PRIVATE -O2 -sASSERTIONS=0)
endif()

# The page, its scripts, and the extracted media sit next to spider.js /
# spider.wasm so the build directory can be served as-is.
file(GLOB SPIDER_WEB_FILES ${WASM_ROOT}/web/*)
add_custom_command(TARGET spider POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSET_DIR} ${CMAKE_BINARY_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${WASM_ROOT}/web ${CMAKE_BINARY_DIR}
    VERBATIM
)
add_custom_target(spider_web ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${WASM_ROOT}/web ${CMAKE_BINARY_DIR}
    DEPENDS ${SPIDER_WEB_FILES}
    COMMENT "Copying web shell into the wasm build directory"
)
