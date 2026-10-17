# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

project(filemanager LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

find_package(CutefishToolkit REQUIRED)
find_package(CutefishFiles REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ICU REQUIRED IMPORTED_TARGET icu-uc icu-i18n)

# ---------------------------------------------------------------------------
# cutefish-mountd: phones and servers mounted with FUSE, so their files are
# local paths to everything else.
# ---------------------------------------------------------------------------
pkg_check_modules(Fuse3 REQUIRED IMPORTED_TARGET fuse3)
pkg_check_modules(LibSsh REQUIRED IMPORTED_TARGET libssh)
pkg_check_modules(SmbClient REQUIRED IMPORTED_TARGET smbclient)
pkg_check_modules(LibMtp REQUIRED IMPORTED_TARGET libmtp)
pkg_check_modules(LibUdev REQUIRED IMPORTED_TARGET libudev)

add_library(mountd-core STATIC
    mountd/fusemount.cpp
    mountd/keyring.cpp
    mountd/mountservice.cpp
    mountd/mtpbackend.cpp
    mountd/mtpmonitor.cpp
    mountd/remoteurl.cpp
    mountd/sftpbackend.cpp
    mountd/smbbackend.cpp
)
target_include_directories(mountd-core PUBLIC mountd)
target_link_libraries(mountd-core PUBLIC
    Cutefish::ToolkitBase
    PkgConfig::Fuse3
    PkgConfig::LibSsh
    PkgConfig::SmbClient
    PkgConfig::LibMtp
    PkgConfig::LibUdev
    PkgConfig::ICU
)

add_executable(cutefish-mountd mountd/main.cpp)
target_link_libraries(cutefish-mountd PRIVATE mountd-core)

configure_file(mountd/com.cutefish.Mounts1.service.in com.cutefish.Mounts1.service @ONLY)

install(TARGETS cutefish-mountd RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.cutefish.Mounts1.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)

# ---------------------------------------------------------------------------
# cutefish-filemanager-daemon: file operations that outlive the windows,
# com.cutefish.FileOperations.
# ---------------------------------------------------------------------------
find_path(LIBZIP_INCLUDE_DIR zip.h)
find_library(LIBZIP_LIBRARY NAMES zip)
if(NOT LIBZIP_INCLUDE_DIR OR NOT LIBZIP_LIBRARY)
    message(FATAL_ERROR "libzip development files are required to build filemanager")
endif()
pkg_check_modules(LibArchive REQUIRED IMPORTED_TARGET libarchive)

add_library(fileoperations-core STATIC
    daemon/archivejob.cpp
    daemon/daemontask.cpp
    daemon/fileoperationsservice.cpp
)
target_include_directories(fileoperations-core PUBLIC ${PROJECT_SOURCE_DIR} daemon ${LIBZIP_INCLUDE_DIR})
target_link_libraries(fileoperations-core PUBLIC Cutefish::ToolkitBase Cutefish::Files ${LIBZIP_LIBRARY}
                      PkgConfig::LibArchive)

add_executable(cutefish-filemanager-daemon daemon/main.cpp)
target_link_libraries(cutefish-filemanager-daemon PRIVATE fileoperations-core)

configure_file(daemon/com.cutefish.FileOperations.service.in com.cutefish.FileOperations.service @ONLY)
install(TARGETS cutefish-filemanager-daemon RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.cutefish.FileOperations.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)

# ---------------------------------------------------------------------------
# The file manager's models: listing, sorting, selection, file operations
# through the daemon. No UI beyond the clipboard.
# ---------------------------------------------------------------------------
add_library(fm-core STATIC
    src/core/config.cpp
    src/core/dates.cpp
    src/core/fileitem.cpp
    src/core/launcher.cpp
    src/core/lister.cpp
    src/core/notifications.cpp
    src/core/preferences.cpp
    src/core/previewkeys.cpp
    src/core/standardfolders.cpp
    src/core/mounts.cpp
    src/core/shortcutconvention.cpp
    src/core/thumbnails.cpp
    src/model/foldercontents.cpp
    src/model/foldermodel.cpp
    src/model/foldersizecache.cpp
    src/model/pathbarmodel.cpp
    src/model/pathhistory.cpp
    src/model/places.cpp
    src/model/positioner.cpp
    src/ops/operations.cpp
)
target_include_directories(fm-core PUBLIC src)
target_link_libraries(fm-core PUBLIC Cutefish::Toolkit Cutefish::Files Cutefish::FilesThumbnails)

# The browser window, its views and dialogs.
add_library(fm-ui STATIC
    src/views/colors.cpp
    src/views/completionpopup.cpp
    src/views/foldermenu.cpp
    src/views/folderpage.cpp
    src/views/folderview.cpp
    src/views/folderwindow.cpp
    src/views/gridview.cpp
    src/views/icongridview.cpp
    src/views/listview.cpp
    src/views/pathbar.cpp
    src/views/previewclient.cpp
    src/views/renameeditor.cpp
    src/views/sidebar.cpp
    src/views/style.cpp
    src/views/tabbar.cpp
    src/dialogs/archiveprogressdialog.cpp
    src/dialogs/framelessdialog.cpp
    src/dialogs/iotaskdialog.cpp
    src/dialogs/openwithdialog.cpp
    src/dialogs/propertiesdialog.cpp
    src/dialogs/serverdialogs.cpp
    src/dialogs/unlockdialog.cpp
    src/chooser/chooserwindow.cpp
    src/desktop/desktopmenuexporter.cpp
    src/desktop/desktopview.cpp
    src/desktop/desktopwindow.cpp
    src/chooser/filechooser.cpp
    src/preview/imageinfo.cpp
    src/preview/imagepreview.cpp
    src/preview/previewcontroller.cpp
    src/preview/previewwindow.cpp
)
target_link_libraries(fm-ui PUBLIC fm-core PkgConfig::ICU)

add_executable(cutefish-filemanager
    src/app/application.cpp
    src/app/chooserservice.cpp
    src/app/dialogs.cpp
    src/app/main.cpp
    src/app/previewservice.cpp
)
target_link_libraries(cutefish-filemanager PRIVATE fm-ui)
install(TARGETS cutefish-filemanager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
configure_file(data/com.cutefish.FileManager1.service.in com.cutefish.FileManager1.service @ONLY)
configure_file(data/com.cutefish.FilePreview.service.in com.cutefish.FilePreview.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.cutefish.FileManager1.service
              ${CMAKE_CURRENT_BINARY_DIR}/com.cutefish.FilePreview.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
install(FILES data/cutefish-filemanager.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
