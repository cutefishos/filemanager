# org.cutefish.filemanager.desktop
#
# Wraps the file manager core as a QML module so a shell can host the desktop
# without depending on KIO. The module installs as a normal Qt QML module, so
# `import org.cutefish.filemanager.desktop 1.0` just works.

set(PLUGIN_TARGET cutefishdesktopplugin)
set(PLUGIN_INSTALL_DIR ${INSTALL_QMLDIR}/org/cutefish/filemanager/desktop)

add_library(${PLUGIN_TARGET} MODULE
    desktopplugin.cpp
    desktopview.cpp
)

target_link_libraries(${PLUGIN_TARGET} PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    cutefish-filemanager-core
)

install(TARGETS ${PLUGIN_TARGET} DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES qmldir DESTINATION ${PLUGIN_INSTALL_DIR})
