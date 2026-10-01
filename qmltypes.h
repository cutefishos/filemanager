// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CUTEFISH_FILEMANAGER_QMLTYPES_H
#define CUTEFISH_FILEMANAGER_QMLTYPES_H

class QQmlEngine;

namespace CutefishFM {

/**
 * Registers every QML type of the file manager core: the "Cutefish.FileManager"
 * and "Cutefish.DragDrop" modules.
 *
 * Both the file manager executable and the desktop QML plugin call this, so the
 * shared QML (FolderGridView, dialogs, the desktop) behaves identically in
 * either host. Calling it more than once is harmless.
 */
void registerQmlTypes();

/**
 * Makes the core's compiled-in resources (":/qml", ":/images", ":/templates")
 * available. Required because the core is a static library: without an explicit
 * reference the linker would drop the resource initialiser.
 */
void initResources();

/**
 * Installs the image providers the shared QML expects ("thumbnailer", "fileicon", and
 * "icontheme") on @p engine, unless they are already present.
 *
 * Qt 6 only calls QQmlExtensionPlugin::initializeEngine() for the first engine
 * that imports a module, so FishUI's own "icontheme" provider is missing from
 * every engine created afterwards.
 */
void registerImageProviders(QQmlEngine *engine);

}

#endif // CUTEFISH_FILEMANAGER_QMLTYPES_H
