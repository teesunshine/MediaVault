cmake_minimum_required(VERSION 3.21)
project(MediaVault VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(MEDIAVAULT_BUILD_TESTS "Build the library scanner tests" ON)
option(MEDIAVAULT_WEBENGINE "Enable web themes (needs Qt WebEngine)" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Widgets Multimedia MultimediaWidgets)

set(MV_WEB OFF)
if(MEDIAVAULT_WEBENGINE)
    find_package(Qt6 6.5 QUIET OPTIONAL_COMPONENTS WebEngineWidgets WebChannel)
    if(TARGET Qt6::WebEngineWidgets AND TARGET Qt6::WebChannel)
        set(MV_WEB ON)
    else()
        message(WARNING "Qt WebEngine not found: building without web themes")
    endif()
endif()

# Scanner and Discord client have no GUI dependencies so they can be tested headless.
add_library(mediavault_core STATIC
    src/MediaLibrary.cpp
    src/MediaLibrary.h
    src/DiscordPresence.cpp
    src/DiscordPresence.h
    src/DiscordSettings.cpp
    src/DiscordSettings.h
    src/ArtworkResolver.cpp
    src/ArtworkResolver.h
    src/ThemeData.cpp
    src/ThemeData.h
)
target_include_directories(mediavault_core PUBLIC src)
target_link_libraries(mediavault_core PUBLIC Qt6::Core Qt6::Network)

qt_add_executable(MediaVault WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/DiscordSettingsDialog.cpp
    src/DiscordSettingsDialog.h
    src/ThemeBridge.cpp
    src/ThemeBridge.h
    src/ThemePanel.cpp
    src/ThemePanel.h
)
target_link_libraries(MediaVault PRIVATE
    mediavault_core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

if(MV_WEB)
    target_link_libraries(MediaVault PRIVATE Qt6::WebEngineWidgets Qt6::WebChannel)
    target_compile_definitions(MediaVault PRIVATE MV_HAVE_WEBENGINE)
endif()

# Bundled themes live in a "themes" folder next to the executable.
add_custom_command(TARGET MediaVault POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/themes $<TARGET_FILE_DIR:MediaVault>/themes
)

if(MEDIAVAULT_BUILD_TESTS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    enable_testing()
    foreach(t medialibrary discord artwork themes)
        qt_add_executable(tst_${t} tests/tst_${t}.cpp)
        target_link_libraries(tst_${t} PRIVATE mediavault_core Qt6::Test)
        add_test(NAME ${t} COMMAND tst_${t})
    endforeach()
endif()

install(TARGETS MediaVault
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
install(DIRECTORY themes/ DESTINATION bin/themes)
