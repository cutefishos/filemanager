cmake_minimum_required(VERSION 3.14)

project(filemanager LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
option(FILEMANAGER_BUILD_TESTS "Build file manager integration tests" OFF)
if(FILEMANAGER_BUILD_TESTS)
    enable_testing()
endif()
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cutefish-framework/io
                 ${CMAKE_CURRENT_BINARY_DIR}/cutefish-framework-io-build)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cutefish-framework/applications
                 ${CMAKE_CURRENT_BINARY_DIR}/cutefish-framework-applications-build)

find_package(Qt6 COMPONENTS Core DBus Quick Widgets LinguistTools REQUIRED)

find_path(LIBZIP_INCLUDE_DIR zip.h)
find_library(LIBZIP_LIBRARY NAMES zip)
if (NOT LIBZIP_INCLUDE_DIR OR NOT LIBZIP_LIBRARY)
    message(FATAL_ERROR "libzip development files are required to build filemanager")
endif ()


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

  cio/cfilesizejob.cpp
  cio/archivejob.cpp

  dialogs/filepropertiesdialog.cpp
  dialogs/openwithdialog.cpp

  widgets/rubberband.cpp
  widgets/itemviewadapter.cpp

  helper/datehelper.cpp
  helper/pathhistory.cpp
  helper/fm.cpp
  helper/iojobui.cpp
  helper/shortcut.cpp
  helper/filelauncher.cpp
  helper/keyboardsearchmanager.cpp

  mimetype/mimeapps.cpp
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
  Cutefish::Applications
  Qt6::Core
  Qt6::DBus
  Qt6::Quick

  Cutefish::IO
  Qt6::Widgets
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

if(FILEMANAGER_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(tst_mimeapps tests/tst_mimeapps.cpp mimetype/mimeapps.cpp)
    target_include_directories(tst_mimeapps PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_mimeapps PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME mimeapps COMMAND tst_mimeapps)
    add_executable(tst_placesmodel tests/tst_placesmodel.cpp model/placesmodel.cpp model/placesitem.cpp)
    target_link_libraries(tst_placesmodel PRIVATE Cutefish::IO Qt6::Test)
    add_test(NAME placesmodel COMMAND tst_placesmodel)
    add_executable(tst_iointegration tests/tst_iointegration.cpp)
    target_link_libraries(tst_iointegration PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME iointegration COMMAND tst_iointegration)
    set_tests_properties(iointegration PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_desktoplayout tests/tst_desktoplayout.cpp)
    target_link_libraries(tst_desktoplayout PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME desktoplayout COMMAND tst_desktoplayout)
    set_tests_properties(desktoplayout PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_tabs tests/tst_tabs.cpp)
    target_link_libraries(tst_tabs PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME tabs COMMAND tst_tabs)
    set_tests_properties(tabs PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_lifecycle tests/tst_lifecycle.cpp application.cpp dbusinterface.cpp ${DBUS_SOURCES})
    target_link_libraries(tst_lifecycle PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME lifecycle COMMAND tst_lifecycle)
    set_tests_properties(lifecycle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
