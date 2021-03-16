cmake_minimum_required(VERSION 3.14)

project(cutefish-filemanager LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Quick Concurrent DBus LinguistTools REQUIRED)
find_package(KF5KIO)
find_package(MeuiKit REQUIRED)

add_executable(cutefish-filemanager
    src/main.cpp
    src/fm.cpp
    src/fmh.cpp
    src/fmstatic.cpp
    src/fmlist.cpp
    src/handy.cpp
    src/placeslist.cpp
    src/pathlist.cpp
    src/baselist.cpp
    src/basemodel.cpp
    src/rubberband.cpp
    src/iconthemeprovider.cpp

    src/lib/foldermodel.cpp
    src/lib/positioner.cpp
    src/lib/itemviewadapter.cpp
    src/lib/fileitemactions.cpp
    src/lib/placesmodel.cpp
    src/lib/placesitem.cpp

    src/dialogs/propertiesdialog.cpp

    src/desktop/desktopview.cpp
    src/desktop/desktopsettings.cpp

    qml.qrc
)

file(GLOB TS_FILES translations/*.ts)
qt5_create_translation(QM_FILES ${TS_FILES})
add_custom_target(translations DEPENDS ${QM_FILES} SOURCES ${TS_FILES})
add_dependencies(${PROJECT_NAME} translations)

target_link_libraries(cutefish-filemanager
    PRIVATE
    Qt5::Core
    Qt5::GuiPrivate
    Qt5::Quick
    Qt5::Concurrent
    Qt5::DBus

    KF5::KIOCore
    KF5::KIOFileWidgets
    KF5::KIOWidgets

    MeuiKit
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION /usr/bin)
install(FILES cutefish-filemanager.desktop DESTINATION "/usr/share/applications")
install(FILES ${QM_FILES} DESTINATION /usr/share/cutefish-filemanager/translations)
