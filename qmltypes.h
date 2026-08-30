/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
 * Installs the image providers the shared QML expects ("thumbnailer" and
 * "icontheme") on @p engine, unless they are already present.
 *
 * Qt 6 only calls QQmlExtensionPlugin::initializeEngine() for the first engine
 * that imports a module, so FishUI's own "icontheme" provider is missing from
 * every engine created afterwards.
 */
void registerImageProviders(QQmlEngine *engine);

}

#endif // CUTEFISH_FILEMANAGER_QMLTYPES_H
