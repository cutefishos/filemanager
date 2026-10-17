// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dialogs/archivepassworddialog.h"
#include "model/foldermodel.h"

#include "ui/cell_layout.h"
#include "ui/dialog_window.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/layout.h"
#include "ui/text_field.h"
#include "ui/theme.h"
#include "ui/window.h"

namespace fm {

class ArchivePasswordDialog::Content : public ui::ColumnLayout
{
public:
    explicit Content(ArchivePasswordDialog* dialog)
        : m_dialog(dialog)
    {
        const ui::Theme& theme = ui::Theme::current();
        setSpacing(12);
        // The dialog pads the sides and the bottom; the top takes 16 more.
        setPadding({0, 16, 0, 0});

        auto* message = addChild<ui::Label>(dialog->m_retry
                                                    ? "The password for “" + dialog->m_name
                                                            + "” is incorrect. Try again."
                                                    : "Enter the password to extract “" + dialog->m_name + "”.");
        message->setWrap(true);
        message->setColor(theme.disabledText);
        message->setLayoutHints({.fillWidth = true});

        password = addChild<ui::TextField>();
        password->setPlaceholder("Password");
        password->setEchoMode(ui::TextField::EchoMode::Password);
        password->setLayoutHints({.fillWidth = true});
        password->onAccepted = [this] { accept(); };
        password->onTextChanged = [this] { extract->setEnabled(!password->text().empty()); };

        auto* actions = addChild<ui::RowLayout>();
        actions->setSpacing(8);
        ui::LayoutHints actionHints {.horizontalAlignment = ui::Alignment::End};
        actionHints.margins.top = 4;
        actions->setLayoutHints(actionHints);
        auto* cancel = actions->addChild<ui::DialogButton>("Cancel");
        cancel->onClicked = [this] { m_dialog->m_dialog->close(); };
        extract = actions->addChild<ui::DialogButton>("Extract", true);
        extract->onClicked = [this] { accept(); };
        extract->setEnabled(false);
    }

    void accept()
    {
        if (password->text().empty())
            return;
        m_dialog->m_answered = true;
        m_dialog->m_model.answerArchivePassword(m_dialog->m_request, password->text());
        m_dialog->m_dialog->close();
    }

    ui::TextField* password;
    ui::DialogButton* extract;

private:
    ArchivePasswordDialog* m_dialog;
};

ArchivePasswordDialog::ArchivePasswordDialog(ui::Window* parent, FolderModel& model, uint32_t request,
                                             std::string name, bool retry)
    : m_model(model)
    , m_request(request)
    , m_name(std::move(name))
    , m_retry(retry)
{
    m_dialog = std::make_unique<ui::DialogWindow>(parent, "Password Required", 400);
    auto* content = m_dialog->setContent<Content>(this);
    m_dialog->onClosed = [this] {
        // Closed without an answer: the extraction is given up.
        if (!m_answered)
            m_model.cancelArchive();
        if (onDone)
            onDone();
    };
    m_dialog->window()->setBackgroundColor(ui::Theme::current().secondBackground);
    m_dialog->window()->addShortcut(ui::NoModifier, ui::key::Escape, [this] { m_dialog->close(); });
    content->password->setFocus();
}

ArchivePasswordDialog::~ArchivePasswordDialog() = default;

void ArchivePasswordDialog::show()
{
    m_dialog->show();
}

void ArchivePasswordDialog::dismiss()
{
    m_answered = true;
    m_dialog->close();
}

} // namespace fm
