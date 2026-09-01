cmake_minimum_required(VERSION 3.14)

project(filemanager LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(Qt6 COMPONENTS Core DBus Quick LinguistTools REQUIRED)

find_path(LIBZIP_INCLUDE_DIR zip.h)
find_library(LIBZIP_LIBRARY NAMES zip)
if (NOT LIBZIP_INCLUDE_DIR OR NOT LIBZIP_LIBRARY)
    message(FATAL_ERROR "libzip development files are required to build filemanager")
endif ()

find_package(KF6KIO)
find_package(KF6Solid)
find_package(KF6WindowSystem)
find_package(KF6Config)
find_package(KF6XmlGui REQUIRED)

# Where QML modules go, so the desktop plugin lands next to FishUI.
find_program(QT_PATHS_EXECUTABLE NAMES qtpaths6 REQUIRED)
execute_process(COMMAND ${QT_PATHS_EXECUTABLE} -query QT_INSTALL_QML
    OUTPUT_VARIABLE INSTALL_QMLDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT INSTALL_QMLDIR)
    message(FATAL_ERROR "qml directory cannot be detected.")
endif ()

# ---------------------------------------------------------------------------
# cutefish-filemanager-core
#
# Everything the file manager knows about files: models, jobs, thumbnails,
# dialogs, drag and drop, and the shared QML. Both the file manager window and
# the desktop QML plugin are built on top of it, so the desktop and the browser
# behave the same and there is only one place to fix a bug.
# ---------------------------------------------------------------------------
add_library(cutefish-filemanager-core STATIC
  qmltypes.cpp

  # FolderModel opens its delete dialog through Window, so the shared code
  # cannot live in the executable alone -- the desktop plugin needs it too.
  window.cpp

  draganddrop/declarativedroparea.cpp
  draganddrop/declarativedragdropevent.cpp
  draganddrop/declarativemimedata.cpp

  model/foldermodel.cpp
  model/placesmodel.cpp
  model/placesitem.cpp
  model/pathbarmodel.cpp
  model/dirlister.cpp
  model/positioner.cpp

  cio/cfilejob.cpp
  cio/cfilesizejob.cpp
  cio/archivejob.cpp

  dialogs/filepropertiesdialog.cpp
  dialogs/openwithdialog.cpp

  widgets/rubberband.cpp
  widgets/itemviewadapter.cpp

  desktop/desktopsettings.cpp
  desktop/dockdbusinterface.cpp

  helper/datehelper.cpp
  helper/pathhistory.cpp
  helper/fm.cpp
  helper/shortcut.cpp
  helper/filelauncher.cpp
  helper/keyboardsearchmanager.cpp

  mimetype/mimeappmanager.cpp
  mimetype/xdgdesktopfile.cpp

  thumbnailer/thumbnailprovider.cpp
  thumbnailer/thumbnailcache.cpp

  desktopiconprovider.cpp

  qml.qrc
)

# The core is linked into a shared plugin, so it has to be position independent.
set_target_properties(cutefish-filemanager-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(cutefish-filemanager-core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(cutefish-filemanager-core
  PUBLIC
  Qt6::Core
  Qt6::DBus
  Qt6::Quick

  KF6::KIOCore
  KF6::KIOFileWidgets
  KF6::KIOWidgets
  KF6::Solid
  KF6::WindowSystem
  KF6::ConfigCore
  KF6::XmlGui
  ${LIBZIP_LIBRARY}
)

target_include_directories(cutefish-filemanager-core PRIVATE ${LIBZIP_INCLUDE_DIR})

# ---------------------------------------------------------------------------
# cutefish-filemanager -- the file browser window
# ---------------------------------------------------------------------------
qt6_add_dbus_adaptor(DBUS_SOURCES
                     com.cutefish.FileManager.xml
                     application.h Application)

add_executable(cutefish-filemanager
  main.cpp
  application.cpp
  dbusinterface.cpp

  ${DBUS_SOURCES}
)

target_link_libraries(cutefish-filemanager PRIVATE cutefish-filemanager-core)

# ---------------------------------------------------------------------------
# The desktop, as a QML module for cutefish-shell
# ---------------------------------------------------------------------------
add_subdirectory(desktopplugin)

file(GLOB TS_FILES translations/*.ts)
qt6_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations DEPENDS ${QM_FILES} SOURCES ${TS_FILES})
add_dependencies(cutefish-filemanager translations)

install(TARGETS cutefish-filemanager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES cutefish-filemanager.desktop DESTINATION "/usr/share/applications")
install(FILES ${QM_FILES} DESTINATION /usr/share/cutefish-filemanager/translations)
