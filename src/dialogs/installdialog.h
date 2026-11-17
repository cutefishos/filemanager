// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "core/common.h"

#include "base/signal.h"
#include "cutefish/packagekit/installer.h"

#include <functional>
#include <memory>
#include <string>

namespace cutefish::ui {
class DialogWindow;
class Window;
}

namespace fm {

// Offers to install the package a file needs before it can open, and shows
// the installation.
class InstallDialog
{
public:
    InstallDialog(ui::Window* parent, std::string package, std::string fileName);
    ~InstallDialog();

    void show();
    void retranslate();
    ui::Window* parentWindow() const { return m_parent; }

    // The package is installed; the dialog closes.
    std::function<void()> onInstalled;
    // No package source of this system has the package.
    std::function<void()> onUnavailable;
    // Closed, whichever way.
    std::function<void()> onDone;

private:
    class Content;
    // What the content shows: the question, the installation, or why it
    // did not happen.
    enum class Page {
        Ask,
        Progress,
        Unavailable,
        Failed,
    };

    void update();
    void dismiss();

    ui::Window* m_parent;
    std::string m_package;
    std::string m_fileName;
    std::unique_ptr<ui::DialogWindow> m_dialog;
    Content* m_content = nullptr;
    Page m_page = Page::Ask;
    cutefish::packagekit::Installer m_installer;
    base::Subscription m_changed;
};

} // namespace fm
