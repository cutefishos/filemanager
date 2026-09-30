#pragma once

#include <QString>

// FolderModel's preferences live in filemanager.conf, which the file chooser shares.
// The desktop, hosted by cutefish-shell, keeps its own in the [Desktop] group of shell.conf.
namespace FolderPreferences
{
void useDesktop();
QString file();
QString group();
}
