cmake_minimum_required(VERSION 3.20)
project(spider_mac LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL3 REQUIRED CONFIG)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(MAC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(REPO_ROOT "${MAC_ROOT}/../.." ABSOLUTE)
set(SPIDER_EXE "${REPO_ROOT}/orig/XPSP1/spider.exe" CACHE FILEPATH "Original spider.exe to take bitmaps and sounds from")
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

# Bundle icon from icon 103, when ffmpeg is available to unpack the .ico.
find_program(FFMPEG_EXE ffmpeg)
find_program(ICONUTIL_EXE iconutil)
set(SPIDER_ICNS ${CMAKE_BINARY_DIR}/Spider.icns)
set(SPIDER_ICON_SOURCE)
if(FFMPEG_EXE AND ICONUTIL_EXE)
    add_custom_command(
        OUTPUT ${SPIDER_ICNS}
        COMMAND sh ${MAC_ROOT}/make_icns.sh ${ASSET_DIR}/icons/103.ico ${SPIDER_ICNS}
        DEPENDS ${ASSET_STAMP} ${MAC_ROOT}/make_icns.sh
        COMMENT "Building Spider.icns"
        VERBATIM
    )
    set_source_files_properties(${SPIDER_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources GENERATED TRUE)
    set(SPIDER_ICON_SOURCE ${SPIDER_ICNS})
else()
    message(STATUS "ffmpeg or iconutil not found: Spider.app will use the default icon")
endif()

add_library(spider_game STATIC ${MAC_ROOT}/game/game.cpp)
target_include_directories(spider_game PUBLIC ${MAC_ROOT}/game ${MAC_ROOT}/host)
target_compile_options(spider_game PRIVATE -Wall -Wextra)

add_executable(spider MACOSX_BUNDLE
    ${MAC_ROOT}/host/main.cpp
    ${MAC_ROOT}/host/audio.cpp
    ${MAC_ROOT}/host/cocoa_host.mm
    ${MAC_ROOT}/render/bmp.cpp
    ${MAC_ROOT}/render/renderer.mm
    ${SPIDER_ICON_SOURCE}
)
add_dependencies(spider spider_assets)
target_include_directories(spider PRIVATE ${MAC_ROOT}/render)
target_link_libraries(spider PRIVATE
    spider_game
    SDL3::SDL3
    "-framework Metal"
    "-framework QuartzCore"
    "-framework CoreText"
    "-framework AppKit"
    "-framework Foundation"
)
target_compile_options(spider PRIVATE -Wall -Wextra $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
set_target_properties(spider PROPERTIES
    OUTPUT_NAME Spider
    MACOSX_BUNDLE_BUNDLE_NAME Spider
    MACOSX_BUNDLE_GUI_IDENTIFIER org.spiderxp.Spider
    MACOSX_BUNDLE_SHORT_VERSION_STRING 5.1.2600.1106
    MACOSX_BUNDLE_BUNDLE_VERSION 5.1.2600.1106
    MACOSX_BUNDLE_ICON_FILE Spider.icns
    BUILD_RPATH "${SDL3_DIR}/../.."
)

add_custom_command(TARGET spider POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSET_DIR} $<TARGET_BUNDLE_CONTENT_DIR:spider>/Resources/assets
    VERBATIM
)

add_executable(spider_test ${MAC_ROOT}/game/game_test.cpp)
target_link_libraries(spider_test PRIVATE spider_game)
target_compile_options(spider_test PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME spider_game COMMAND spider_test)
