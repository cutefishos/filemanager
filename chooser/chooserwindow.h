// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class FileChooser;
class QQuickWindow;
class WindowManager;

/** Compiles the dialog ahead of its first openChooserWindow(). */
bool preloadChooserWindow(WindowManager *windows);

/**
 * Creates the dialog for @p chooser in @p windows' engine and shows it on the
 * next event loop turn, parented onto the window its request names. Returns
 * nullptr if the QML cannot be created.
 */
QQuickWindow *openChooserWindow(WindowManager *windows, FileChooser *chooser);
