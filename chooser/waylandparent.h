#pragma once

#include <QString>

class QWindow;

// Parents the dialog onto the requesting application's window, which lives in
// another process: the portal hands over a "wayland:<handle>" exported through
// xdg-foreign. Does nothing on other platforms or without a usable handle.
void applyWaylandParent(QWindow *window, const QString &parentWindow, bool modal);
