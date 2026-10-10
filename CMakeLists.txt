cmake_minimum_required(VERSION 3.16)
project(SGP VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(XCB QUIET IMPORTED_TARGET xcb)

set(SOURCES
    src/main.cpp
    src/PerfilVisual.cpp
    src/x11util.cpp
    src/x11windowutils.cpp
    src/DockWindow.cpp
    src/DockItem.cpp
    src/ApplicationScanner.cpp
    src/SettingsDialog.cpp
    src/AppLauncher.cpp
    src/AppMenu.cpp
    src/VentanaPopup.cpp
    src/DesktopPopup.cpp
    src/DockAnimation.cpp
    src/SystemTrayArea.cpp
    src/TrayHost.cpp
    src/TrashItem.cpp
    src/SeparatorItem.cpp
    src/SystemPanel.cpp
    resources.qrc
)

set(HEADERS
    src/DockWindow.h
    src/DockItem.h
    src/ApplicationScanner.h
    src/SettingsDialog.h
    src/DockConfig.h
    src/PerfilVisual.h
    src/x11_helper.h
    src/x11windowutils.h
    src/AppLauncher.h
    src/AppMenu.h
    src/VentanaPopup.h
    src/DesktopPopup.h
    src/DockAnimation.h
    src/SystemTrayArea.h
    src/TrayHost.h
    src/TrashItem.h
    src/SeparatorItem.h
    src/SystemPanel.h
)

add_executable(sgp ${SOURCES} ${HEADERS})

if(XCB_FOUND)
    target_link_libraries(sgp PkgConfig::XCB)
endif()

target_link_libraries(sgp Qt6::Core Qt6::Gui Qt6::Widgets Qt6::DBus ${X11_LIBRARIES})

target_include_directories(sgp PRIVATE src ${X11_INCLUDE_DIRS})

# Qt6 private headers for QPA native interface (X11 display access)
get_target_property(Qt6_GuiIncludes Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir ${Qt6_GuiIncludes})
    if(EXISTS "${dir}/${Qt6_VERSION}/QtGui")
        target_include_directories(sgp PRIVATE "${dir}/${Qt6_VERSION}/QtGui")
    endif()
endforeach()

install(TARGETS sgp DESTINATION bin)
install(FILES sgp.desktop DESTINATION share/applications)
