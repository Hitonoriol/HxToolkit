cmake_minimum_required(VERSION 3.21)

project(HxNxToolkit LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(HXNXTOOLKIT_OUTPUT_DIR ${CMAKE_BINARY_DIR})

file(GLOB HXNXTOOLKIT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.ui
)

foreach(source_dir App Enums Filesystem General Gitlab JSON Productivity Random System Time UI Util)
    file(GLOB_RECURSE directory_sources CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/${source_dir}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/${source_dir}/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/${source_dir}/*.ui
    )
    list(APPEND HXNXTOOLKIT_SOURCES ${directory_sources})
endforeach()

add_library(tinyexpr STATIC
    3rdParty/tinyexpr/tinyexpr.c
    3rdParty/tinyexpr/tinyexpr.h
)

target_include_directories(tinyexpr PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty)

set_target_properties(tinyexpr PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${HXNXTOOLKIT_OUTPUT_DIR}/$<CONFIG>
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    FOLDER Libraries
)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Gui Network Widgets Concurrent)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Widgets Concurrent)

add_subdirectory(3rdParty/qlementine)

if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(qlementine PRIVATE /W3 /WX-)
endif()

add_executable(HxNxToolkit WIN32
    ${HXNXTOOLKIT_SOURCES}
    Assets/Resources.qrc
    Assets/AppIcon.rc
)

target_include_directories(HxNxToolkit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty
)

target_compile_definitions(HxNxToolkit PRIVATE UNICODE _UNICODE)

target_link_libraries(HxNxToolkit PRIVATE
    tinyexpr
    qlementine
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    Qt6::Concurrent
)

set_target_properties(HxNxToolkit PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${HXNXTOOLKIT_OUTPUT_DIR}/$<CONFIG>
    LIBRARY_OUTPUT_DIRECTORY ${HXNXTOOLKIT_OUTPUT_DIR}/$<CONFIG>
    RUNTIME_OUTPUT_DIRECTORY ${HXNXTOOLKIT_OUTPUT_DIR}/$<CONFIG>
    FOLDER Apps
)

if(MSVC)
    target_compile_options(HxNxToolkit PRIVATE /permissive- /sdl /W3 /EHsc)
endif()
