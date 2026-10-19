// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

namespace fm {

// ~/.config/cutefish/filemanager.conf, the file manager's settings, which
// this process alone writes.
std::string settingsFile();
// Another file for every setting, for tests and tools; before first use.
void setSettingsFile(std::string path);

} // namespace fm
