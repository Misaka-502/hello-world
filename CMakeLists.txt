cmake_minimum_required(VERSION 3.16)

project(MusicPlayer VERSION 1.0.0 LANGUAGES CXX)

# ============================================================
# 1. Find required Qt packages
# ============================================================

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

if(${QT_VERSION_MAJOR} EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Multimedia)
else()
    find_package(Qt5Multimedia REQUIRED)
endif()

# ============================================================
# 2. Set C++ standard
# ============================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ============================================================
# 3. Source files (with src/ prefix)
# ============================================================

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/track.cpp
    src/playlist.cpp
    src/library.cpp
    src/player.cpp
    src/lrcparser.cpp
    src/lyricswidget.cpp
    src/transcoder.cpp
    src/lyricsrenderer.cpp
    src/spectrumwidget.cpp
    src/playlistmanager.cpp
    src/playmodestrategy.cpp
)

set(HEADERS
    src/mainwindow.h
    src/track.h
    src/playlist.h
    src/library.h
    src/player.h
    src/lrcparser.h
    src/lyricswidget.h
    src/transcoder.h
    src/lyricsrenderer.h
    src/spectrumwidget.h
    src/playlistmanager.h
    src/playmodestrategy.h
)

# ============================================================
# 4. Create executable
# ============================================================

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# ============================================================
# 5. Link Qt libraries
# ============================================================

if(${QT_VERSION_MAJOR} EQUAL 6)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt5::Widgets
        Qt5::Multimedia
    )
endif()

# ============================================================
# 6. Include directories
# ============================================================

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================
# 7. Print configuration info
# ============================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Qt version: ${QT_VERSION}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "========================================")
message(STATUS "")
