#include "folderpreferences.h"

#include <preferences.h>

namespace
{
bool s_desktop = false;
}

void FolderPreferences::useDesktop()
{
    s_desktop = true;
}

QString FolderPreferences::file()
{
    return Preferences::file(s_desktop ? QStringLiteral("shell") : QStringLiteral("filemanager"));
}

QString FolderPreferences::group()
{
    return s_desktop ? QStringLiteral("Desktop") : QString();
}
