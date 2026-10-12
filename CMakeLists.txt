cmake_minimum_required(VERSION 3.14)

# C is needed for the generated Wayland protocol marshalling.
project(filemanager LANGUAGES C CXX)

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
find_package(Qt6 COMPONENTS Core Concurrent DBus Quick QuickControls2 Widgets LinguistTools REQUIRED)
find_package(CutefishFramework REQUIRED COMPONENTS IO Application)
find_package(ICU REQUIRED COMPONENTS uc i18n)

find_path(LIBZIP_INCLUDE_DIR zip.h)
find_library(LIBZIP_LIBRARY NAMES zip)
if (NOT LIBZIP_INCLUDE_DIR OR NOT LIBZIP_LIBRARY)
    message(FATAL_ERROR "libzip development files are required to build filemanager")
endif ()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LibArchive QUIET IMPORTED_TARGET libarchive)
if (NOT LibArchive_FOUND)
    message(FATAL_ERROR "libarchive development files are required to build filemanager")
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

  # Every host of the shared QML (the file manager, the desktop plugin) opens
  # the dialogs through its own engine's manager.
  windowmanager.cpp

  draganddrop/declarativedroparea.cpp
  draganddrop/declarativedragdropevent.cpp

  model/foldermodel.cpp
  model/foldercontentsmodel.cpp
  model/folderoperations.cpp
  model/folderinteraction.cpp
  model/foldersizecache.cpp
  model/placesmodel.cpp
  model/placesitem.cpp
  model/pathbarmodel.cpp
  model/pathcompletionmodel.cpp
  model/dirlister.cpp
  model/positioner.cpp

  cio/cfilesizejob.cpp
  cio/archivejob.cpp

  dialogs/fileproperties.cpp

  widgets/itemviewadapter.cpp

  helper/datehelper.cpp
  helper/dialogs.cpp
  helper/pathhistory.cpp
  helper/fm.cpp
  helper/mountsclient.cpp
  helper/iojobui.cpp
  helper/previewcontroller.cpp
  helper/previewclient.cpp
  helper/shortcut.cpp
  helper/shortcutconvention.cpp
  helper/filelauncher.cpp
  helper/folderpreferences.cpp

  operations/operationtask.cpp
  operations/fileoperationsclient.cpp

  mimetype/mimeappmanager.cpp
  mimetype/xdgdesktopfile.cpp

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
  Cutefish::Application
  Qt6::Concurrent
  Qt6::Core
  Qt6::DBus
  Qt6::Quick

  Cutefish::IO
  Qt6::Widgets
  ${LIBZIP_LIBRARY}

  PRIVATE
  ICU::i18n
  ICU::uc
  PkgConfig::LibArchive
)

target_include_directories(cutefish-filemanager-core PRIVATE ${LIBZIP_INCLUDE_DIR})

# ---------------------------------------------------------------------------
# The portal's file chooser dialog, shown by the file manager daemon
# ---------------------------------------------------------------------------
add_library(cutefish-filechooser-ui STATIC
  chooser/filechooser.cpp
  chooser/chooserwindow.cpp
  chooser/chooserservice.cpp
  chooser/waylandparent.cpp
)
target_link_libraries(cutefish-filechooser-ui PUBLIC cutefish-filemanager-core Qt6::QuickControls2)

# The dialog never runs in the requesting application, so its window can only
# be reached through xdg-foreign; the protocol code is optional at build time.
pkg_check_modules(WaylandClient QUIET IMPORTED_TARGET wayland-client)
pkg_check_modules(WaylandProtocols QUIET wayland-protocols>=1.32)
find_package(Qt6 QUIET COMPONENTS GuiPrivate)
if(WaylandClient_FOUND AND WaylandProtocols_FOUND AND TARGET Qt6::GuiPrivate)
    pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
    set(CHOOSER_PROTOCOL_DIR ${CMAKE_BINARY_DIR}/chooser-protocols)
    file(MAKE_DIRECTORY ${CHOOSER_PROTOCOL_DIR})
    foreach(protocol xdg-shell:stable/xdg-shell
                     xdg-dialog-v1:staging/xdg-dialog
                     xdg-foreign-unstable-v2:unstable/xdg-foreign)
        string(REPLACE ":" ";" parts ${protocol})
        list(GET parts 0 name)
        list(GET parts 1 directory)
        set(xml ${WAYLAND_PROTOCOLS_DATADIR}/${directory}/${name}.xml)
        add_custom_command(OUTPUT ${CHOOSER_PROTOCOL_DIR}/${name}-protocol.c
            COMMAND ${WAYLAND_SCANNER} private-code ${xml} ${CHOOSER_PROTOCOL_DIR}/${name}-protocol.c
            DEPENDS ${xml} VERBATIM)
        add_custom_command(OUTPUT ${CHOOSER_PROTOCOL_DIR}/${name}-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} client-header ${xml} ${CHOOSER_PROTOCOL_DIR}/${name}-client-protocol.h
            DEPENDS ${xml} VERBATIM)
        target_sources(cutefish-filechooser-ui PRIVATE
            ${CHOOSER_PROTOCOL_DIR}/${name}-protocol.c
            ${CHOOSER_PROTOCOL_DIR}/${name}-client-protocol.h)
    endforeach()
    target_compile_definitions(cutefish-filechooser-ui PRIVATE CUTEFISH_FILECHOOSER_WAYLAND)
    target_include_directories(cutefish-filechooser-ui PRIVATE ${CHOOSER_PROTOCOL_DIR})
    target_link_libraries(cutefish-filechooser-ui PRIVATE PkgConfig::WaylandClient Qt6::GuiPrivate)
endif()

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
  preview/previewservice.cpp
  chooser/chooser.qrc

  ${DBUS_SOURCES}
)

target_link_libraries(cutefish-filemanager PRIVATE cutefish-filechooser-ui)

configure_file(preview/com.cutefish.FilePreview.service.in com.cutefish.FilePreview.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.cutefish.FilePreview.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)

add_executable(cutefish-filemanager-daemon
  daemon/main.cpp
  daemon/fileoperationsservice.cpp
  daemon/daemontask.cpp
  daemon/daemonarchivetask.cpp
  cio/archivejob.cpp
)
target_include_directories(cutefish-filemanager-daemon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(cutefish-filemanager-daemon PRIVATE ${LIBZIP_INCLUDE_DIR})
target_link_libraries(cutefish-filemanager-daemon PRIVATE
  Cutefish::IO
  Qt6::Core
  Qt6::DBus
  ${LIBZIP_LIBRARY}
  PkgConfig::LibArchive
)

configure_file(daemon/com.cutefish.FileOperations.service.in
               com.cutefish.FileOperations.service @ONLY)

# Phones and servers mounted with FUSE, so their files are local paths to everything else.
pkg_check_modules(Fuse3 REQUIRED IMPORTED_TARGET fuse3)
pkg_check_modules(LibSsh REQUIRED IMPORTED_TARGET libssh)
pkg_check_modules(SmbClient REQUIRED IMPORTED_TARGET smbclient)
pkg_check_modules(LibMtp REQUIRED IMPORTED_TARGET libmtp)
pkg_check_modules(LibUdev REQUIRED IMPORTED_TARGET libudev)

add_executable(cutefish-mountd
  mountd/main.cpp
  mountd/mountservice.cpp
  mountd/fusemount.cpp
  mountd/keyring.cpp
  mountd/sftpbackend.cpp
  mountd/smbbackend.cpp
  mountd/mtpbackend.cpp
  mountd/mtpmonitor.cpp
)
target_link_libraries(cutefish-mountd PRIVATE
  Qt6::Core
  Qt6::DBus
  PkgConfig::Fuse3
  PkgConfig::LibSsh
  PkgConfig::SmbClient
  PkgConfig::LibMtp
  PkgConfig::LibUdev
)

configure_file(mountd/com.cutefish.Mounts1.service.in com.cutefish.Mounts1.service @ONLY)

# ---------------------------------------------------------------------------
# The desktop, as a QML module for cutefish-shell
# ---------------------------------------------------------------------------
add_subdirectory(desktopplugin)

file(GLOB TS_FILES translations/*.ts)
qt6_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations DEPENDS ${QM_FILES} SOURCES ${TS_FILES})
add_dependencies(cutefish-filemanager translations)

install(TARGETS cutefish-filemanager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
configure_file(com.cutefish.FileManager1.service.in com.cutefish.FileManager1.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.cutefish.FileManager1.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
install(TARGETS cutefish-filemanager-daemon RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(TARGETS cutefish-mountd RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.cutefish.Mounts1.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.cutefish.FileOperations.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
install(FILES cutefish-filemanager.desktop DESTINATION "/usr/share/applications")
install(FILES ${QM_FILES} DESTINATION /usr/share/cutefish-filemanager/translations)

if(FILEMANAGER_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    find_package(Qt6 REQUIRED COMPONENTS QuickTest)
    add_executable(tst_contextmenu tests/tst_contextmenu.cpp)
    target_link_libraries(tst_contextmenu PRIVATE Qt6::QuickTest Qt6::Quick)
    add_test(NAME contextmenu COMMAND tst_contextmenu -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/menu)
    set_tests_properties(contextmenu PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_openwith tests/tst_openwith.cpp)
    target_link_libraries(tst_openwith PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME openwith COMMAND tst_openwith)
    add_executable(tst_preview tests/tst_preview.cpp)
    target_link_libraries(tst_preview PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME preview COMMAND tst_preview)
    set_tests_properties(preview PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_foldercontents tests/tst_foldercontents.cpp)
    target_link_libraries(tst_foldercontents PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME foldercontents COMMAND tst_foldercontents)
    set_tests_properties(foldercontents PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CUTEFISH_FILEOPERATIONS_IN_PROCESS=1")
    add_executable(tst_responsiveness tests/tst_responsiveness.cpp)
    target_link_libraries(tst_responsiveness PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME responsiveness COMMAND tst_responsiveness)
    set_tests_properties(responsiveness PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CUTEFISH_FILEOPERATIONS_IN_PROCESS=1")
    add_executable(tst_filechooser tests/tst_filechooser.cpp chooser/chooser.qrc)
    target_link_libraries(tst_filechooser PRIVATE cutefish-filechooser-ui Qt6::Test)
    add_test(NAME filechooser COMMAND tst_filechooser)
    set_tests_properties(filechooser PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_desktopicons tests/tst_desktopicons.cpp)
    target_link_libraries(tst_desktopicons PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME desktopicons COMMAND tst_desktopicons)
    set_tests_properties(desktopicons PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(tst_shortcut tests/tst_shortcut.cpp)
    target_link_libraries(tst_shortcut PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME shortcut COMMAND tst_shortcut)
    set_tests_properties(shortcut PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(tst_placesmodel tests/tst_placesmodel.cpp)
    target_link_libraries(tst_placesmodel PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME placesmodel COMMAND tst_placesmodel)
    add_executable(tst_iointegration tests/tst_iointegration.cpp)
    target_link_libraries(tst_iointegration PRIVATE cutefish-filemanager-core Qt6::Test PkgConfig::LibArchive)
    add_test(NAME iointegration COMMAND tst_iointegration)
    set_tests_properties(iointegration PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_desktoplayout tests/tst_desktoplayout.cpp desktopplugin/desktopview.cpp)
    target_link_libraries(tst_desktoplayout PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME desktoplayout COMMAND tst_desktoplayout)
    set_tests_properties(desktoplayout PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_tabs tests/tst_tabs.cpp)
    target_link_libraries(tst_tabs PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME tabs COMMAND tst_tabs)
    set_tests_properties(tabs PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_gridresize tests/tst_gridresize.cpp)
    target_link_libraries(tst_gridresize PRIVATE cutefish-filemanager-core Qt6::Test)
    add_test(NAME gridresize COMMAND tst_gridresize)
    set_tests_properties(gridresize PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_windowmanager tests/tst_windowmanager.cpp)
    target_link_libraries(tst_windowmanager PRIVATE cutefish-filemanager-core Qt6::Test)
    target_compile_definitions(tst_windowmanager PRIVATE
        TEST_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/windowmanager")
    add_test(NAME windowmanager COMMAND tst_windowmanager)
    set_tests_properties(windowmanager PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(tst_lifecycle tests/tst_lifecycle.cpp application.cpp dbusinterface.cpp
                  preview/previewservice.cpp chooser/chooser.qrc ${DBUS_SOURCES})
    target_link_libraries(tst_lifecycle PRIVATE cutefish-filechooser-ui Qt6::Test)
    add_test(NAME lifecycle COMMAND tst_lifecycle windows)
    add_test(NAME lifecycle-task COMMAND tst_lifecycle task)
    add_test(NAME lifecycle-emptytrash COMMAND tst_lifecycle emptytrash)
    find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
    add_test(NAME lifecycle-activation-list
             COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:tst_lifecycle> activation-list)
    add_test(NAME lifecycle-activation-grid
             COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:tst_lifecycle> activation-grid)
    add_test(NAME lifecycle-activation-folders
             COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:tst_lifecycle> activation-folders)
    add_test(NAME lifecycle-daemon
             COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:tst_lifecycle> daemon)
    add_test(NAME lifecycle-daemon-chooser
             COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:tst_lifecycle> daemon-chooser)
    add_test(NAME lifecycle-remote-task
             COMMAND ${DBUS_RUN_SESSION_EXECUTABLE}
                     --config-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/dbus-session.conf
                     -- $<TARGET_FILE:tst_lifecycle> remote-task)
    set_tests_properties(lifecycle lifecycle-task lifecycle-emptytrash
                         lifecycle-activation-list lifecycle-activation-grid
                         lifecycle-activation-folders lifecycle-daemon lifecycle-daemon-chooser
                         lifecycle-remote-task PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

    find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)
    add_executable(tst_dbusactivation tests/tst_dbusactivation.cpp)
    target_link_libraries(tst_dbusactivation PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    target_compile_definitions(tst_dbusactivation PRIVATE
        SERVICE_TEMPLATE="${CMAKE_CURRENT_SOURCE_DIR}/com.cutefish.FileManager1.service.in"
        FILEMANAGER_EXECUTABLE="$<TARGET_FILE:cutefish-filemanager>"
        DBUS_DAEMON_EXECUTABLE="${DBUS_DAEMON_EXECUTABLE}")
    add_dependencies(tst_dbusactivation cutefish-filemanager)
    add_test(NAME dbusactivation COMMAND tst_dbusactivation)
    set_tests_properties(dbusactivation PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;CUTEFISH_FILEOPERATIONS_IN_PROCESS=1")
    add_executable(tst_fileoperationsdaemon tests/tst_fileoperationsdaemon.cpp)
    target_link_libraries(tst_fileoperationsdaemon PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    add_dependencies(tst_fileoperationsdaemon cutefish-filemanager-daemon)
    add_dependencies(tst_lifecycle cutefish-filemanager-daemon)
    # A bus without service directories, so the installed daemon cannot be
    # activated before the build-tree one owns the name.
    add_test(NAME fileoperationsdaemon
             COMMAND ${DBUS_RUN_SESSION_EXECUTABLE}
                     --config-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/dbus-session.conf
                     -- $<TARGET_FILE:tst_fileoperationsdaemon>)
    set_property(TEST desktopicons placesmodel iointegration desktoplayout tabs gridresize windowmanager
                 lifecycle lifecycle-task lifecycle-emptytrash
                 lifecycle-activation-list lifecycle-activation-grid lifecycle-activation-folders
                 lifecycle-daemon
                 APPEND PROPERTY ENVIRONMENT "CUTEFISH_FILEOPERATIONS_IN_PROCESS=1")
endif()
