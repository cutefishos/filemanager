// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dialogs/unlockdialog.h"
#include "model/places.h"

#include "ui/cell_layout.h"
#include "ui/dialog_window.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/layout.h"
#include "ui/text_field.h"
#include "ui/theme.h"
#include "ui/window.h"

namespace fm {

class UnlockVolumeDialog::Content : public ui::ColumnLayout
{
public:
    explicit Content(UnlockVolumeDialog* dialog)
        : m_dialog(dialog)
    {
        setSpacing(12);
        // The dialog pads the sides and the bottom; the top takes 16 more.
        setPadding({0, 16, 0, 0});

        auto* message = addChild<ui::Label>(dialog->m_retry
                                                    ? "The passphrase for “" + dialog->m_name
                                                            + "” is incorrect. Try again."
                                                    : "Enter the passphrase to unlock “" + dialog->m_name + "”.");
        message->setWrap(true);
        message->setColor(ui::ColorId::DisabledText);
        message->setLayoutHints({.fillWidth = true});

        passphrase = addChild<ui::TextField>();
        passphrase->setPlaceholder("Passphrase");
        passphrase->setEchoMode(ui::TextField::EchoMode::Password);
        passphrase->setLayoutHints({.fillWidth = true});
        passphrase->onAccepted = [this] { accept(); };
        passphrase->onTextChanged = [this] { unlock->setEnabled(!passphrase->text().empty()); };

        auto* actions = addChild<ui::RowLayout>();
        actions->setSpacing(8);
        ui::LayoutHints actionHints {.horizontalAlignment = ui::Alignment::End};
        actionHints.margins.top = 4;
        actions->setLayoutHints(actionHints);
        auto* cancel = actions->addChild<ui::DialogButton>("Cancel");
        cancel->onClicked = [this] { m_dialog->m_dialog->close(); };
        unlock = actions->addChild<ui::DialogButton>("Unlock", true);
        unlock->onClicked = [this] { accept(); };
        unlock->setEnabled(false);
    }

    void accept()
    {
        if (passphrase->text().empty())
            return;
        m_dialog->m_places.answerPassphrase(m_dialog->m_udi, passphrase->text());
        m_dialog->m_dialog->close();
    }

    ui::TextField* passphrase;
    ui::DialogButton* unlock;

private:
    UnlockVolumeDialog* m_dialog;
};

UnlockVolumeDialog::UnlockVolumeDialog(ui::Window* parent, PlacesModel& places, std::string udi, std::string name,
                                       bool retry)
    : m_places(places)
    , m_udi(std::move(udi))
    , m_name(std::move(name))
    , m_retry(retry)
{
    m_dialog = std::make_unique<ui::DialogWindow>(parent, "Unlock Volume", 400);
    auto* content = m_dialog->setContent<Content>(this);
    m_dialog->onClosed = [this] {
        if (onDone)
            onDone();
    };
    m_dialog->window()->setBackgroundColor(ui::ColorId::SecondBackground);
    m_dialog->window()->addShortcut(ui::NoModifier, ui::key::Escape, [this] { m_dialog->close(); });
    content->passphrase->setFocus();
}

UnlockVolumeDialog::~UnlockVolumeDialog() = default;

void UnlockVolumeDialog::show()
{
    m_dialog->show();
}

} // namespace fm
