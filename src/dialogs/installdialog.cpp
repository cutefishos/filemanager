// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "dialogs/installdialog.h"

#include "dialogs/progressrow.h"

#include "base/i18n.h"
#include "ui/cell_layout.h"
#include "ui/dialog_window.h"
#include "ui/header_button.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/layout.h"
#include "ui/progress_bar.h"
#include "ui/theme.h"
#include "ui/window.h"

using cutefish::i18n::format;
using cutefish::i18n::tr;
using cutefish::packagekit::Installer;

namespace fm {

class InstallDialog::Content : public ui::ColumnLayout
{
public:
    explicit Content(InstallDialog* dialog)
        : m_dialog(dialog)
    {
        setSpacing(12);

        message = addChild<ui::Label>();
        message->setWrap(true);
        message->setColor(ui::ColorId::DisabledText);
        message->setLayoutHints({.fillWidth = true});

        const Page page = dialog->m_page;
        if (page == Page::Progress) {
            auto* row = addChild<ProgressRow>();
            ui::LayoutHints rowHints {.fillWidth = true};
            rowHints.margins.bottom = 8;
            row->setLayoutHints(rowHints);
            progress = row->progress();
            stop = row->stop();
            stop->onClicked = [this] { m_dialog->dismiss(); };
        } else {
            auto* actions = addChild<ui::RowLayout>();
            actions->setSpacing(8);
            ui::LayoutHints actionHints {.horizontalAlignment = ui::Alignment::End};
            actionHints.margins.top = 4;
            actions->setLayoutHints(actionHints);
            if (page == Page::Ask) {
                auto* cancel = actions->addChild<ui::DialogButton>(tr("Cancel"));
                cancel->onClicked = [this] { m_dialog->dismiss(); };
                auto* install = actions->addChild<ui::DialogButton>(tr("Install"), true);
                install->onClicked = [this] { m_dialog->m_installer.install(m_dialog->m_package); };
            } else {
                auto* done = actions->addChild<ui::DialogButton>(tr("OK"), true);
                done->onClicked = [this] { m_dialog->m_dialog->close(); };
            }
        }
        refresh();
    }

    void refresh()
    {
        const Installer& installer = m_dialog->m_installer;
        switch (m_dialog->m_page) {
        case Page::Ask:
            message->setText(format(tr("To open “{name}”, Reef has to be downloaded and "
                                       "installed."),
                                    {{"name", m_dialog->m_fileName}}));
            break;
        case Page::Progress:
            message->setText(installer.state() == Installer::State::Installing    ? tr("Installing…")
                             : installer.state() == Installer::State::Downloading ? tr("Downloading…")
                                                                                  : tr("Looking for Reef…"));
            progress->setIndeterminate(installer.progress() < 0);
            progress->setValue(installer.progress() < 0 ? 0 : installer.progress() / 100.0);
            stop->setEnabled(installer.canCancel());
            break;
        case Page::Unavailable:
            message->setText(tr("Reef is not available for this computer."));
            break;
        case Page::Failed:
            message->setText(installer.error().empty() ? tr("Reef could not be installed.")
                                                       : installer.error());
            break;
        }
    }

    ui::Label* message;
    ui::ProgressBar* progress = nullptr;
    ui::HeaderButton* stop = nullptr;

private:
    InstallDialog* m_dialog;
};

InstallDialog::InstallDialog(ui::Window* parent, std::string package, std::string fileName)
    : m_parent(parent)
    , m_package(std::move(package))
    , m_fileName(std::move(fileName))
{
    m_dialog = std::make_unique<ui::DialogWindow>(parent, tr("Install Reef"), 400);
    // Cancel, Stop or OK closes it.
    m_dialog->setCloseVisible(false);
    m_content = m_dialog->setContent<Content>(this);
    m_dialog->onClosed = [this] {
        if (onDone)
            onDone();
    };
    m_dialog->window()->setBackgroundColor(ui::ColorId::SecondBackground);
    // From the desktop or the dock it has no window to belong to, and stays
    // off the switcher and the menu bar.
    if (!parent)
        m_dialog->window()->setSkipTaskbar(true);
    m_dialog->window()->addShortcut(ui::NoModifier, ui::key::Escape, [this] { dismiss(); });
    m_changed = m_installer.changed.connect([this] { update(); });
}

InstallDialog::~InstallDialog() = default;

void InstallDialog::show()
{
    m_dialog->show();
}

void InstallDialog::retranslate()
{
    // Closed: it goes once the event is over.
    if (!m_dialog->window())
        return;
    m_dialog->setTitle(tr("Install Reef"));
    m_content = m_dialog->setContent<Content>(this);
}

void InstallDialog::update()
{
    if (!m_dialog->window())
        return;
    Page page = Page::Ask;
    switch (m_installer.state()) {
    case Installer::State::Installed:
        if (onInstalled)
            onInstalled();
        m_dialog->close();
        return;
    case Installer::State::Looking:
    case Installer::State::Downloading:
    case Installer::State::Installing:
        page = Page::Progress;
        break;
    case Installer::State::Unavailable:
        page = Page::Unavailable;
        if (onUnavailable)
            onUnavailable();
        break;
    case Installer::State::Error:
        page = Page::Failed;
        break;
    case Installer::State::Idle:
        break;
    }
    if (page == m_page) {
        m_content->refresh();
        return;
    }
    m_page = page;
    m_content = m_dialog->setContent<Content>(this);
}

// An installation that can no longer be stopped keeps its dialog.
void InstallDialog::dismiss()
{
    if (!m_installer.isBusy())
        m_dialog->close();
    else if (m_installer.canCancel())
        m_installer.cancel();
}

} // namespace fm
