cmake_minimum_required(VERSION 3.16)

project(Hex_Editor_V01 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    home.cpp
    home.h
    menubar.cpp
    menubar.h
    textconverter.cpp
    textconverter.h
    textanalyzer.cpp
    textanalyzer.h
    highlighter.cpp
    highlighter.h
    codeeditor.cpp
    codeeditor.h
    Resours.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Hex_Editor_V01
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(Hex_Editor_V01 ${PROJECT_SOURCES})
endif()


target_link_libraries(Hex_Editor_V01 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(${QT_VERSION_MAJOR} EQUAL 6)
    qt_finalize_executable(Hex_Editor_V01)
endif()
