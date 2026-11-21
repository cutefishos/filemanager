// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "dialogs/unlockdialog.h"
#include "model/places.h"

#include "base/i18n.h"
#include "ui/cell_layout.h"
#include "ui/dialog_window.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/layout.h"
#include "ui/progress_bar.h"
#include "ui/text_field.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>

using cutefish::i18n::format;
using cutefish::i18n::tr;

namespace fm {

static constexpr double kBusySize = 16;
static constexpr double kBusyGap = 8;

// The busy indicator, when shown, and the text after it, the capitals'
// middle on the indicator's middle.
class StatusLine : public ui::View
{
public:
    StatusLine()
    {
        busy = addChild<ui::BusyIndicator>();
        text = addChild<ui::Label>();
        text->setName("unlock:status");
        text->setWrap(true);
    }

    double heightForWidth(double width) const override
    {
        const double textHeight = text->heightForWidth(std::max(0.0, width - textStart()));
        // Below the indicator's top by up to a device pixel more than this.
        const double textTop = kBusySize / 2 + text->capHeight() / 2 - text->baselineOffset();
        return std::ceil(std::max(kBusySize, std::max(0.0, textTop) + textHeight + 1));
    }

    ui::BusyIndicator* busy;
    ui::Label* text;

protected:
    void layout() override
    {
        // In device pixels from the window's top, as both are drawn on
        // them: the indicator whole, the baseline on a pixel.
        const double width = geometry().width;
        const double scale = window() ? window()->scale() : 1;
        const double origin = mapToWindow({}).y * scale;
        const double side = std::max(1.0, double(std::lround(kBusySize * scale)));
        const double busyTop = std::ceil(origin);
        busy->setGeometry({0, (busyTop - origin) / scale, side / scale, side / scale});
        const double baseline = std::round(busyTop + side / 2 + text->capHeight() * scale / 2);
        const double x = textStart();
        const double textWidth = std::max(0.0, width - x);
        text->setGeometry({x, std::max(0.0, (baseline - origin) / scale - text->baselineOffset()), textWidth,
                           text->heightForWidth(textWidth)});
    }

private:
    double textStart() const { return busy->isVisible() ? kBusySize + kBusyGap : 0; }
};

class UnlockVolumeDialog::Content : public ui::ColumnLayout
{
public:
    explicit Content(UnlockVolumeDialog* dialog)
        : m_dialog(dialog)
    {
        setSpacing(12);

        auto* message = addChild<ui::Label>(
                format(tr("Enter the passphrase to unlock “{name}”."), {{"name", dialog->m_name}}));
        message->setWrap(true);
        message->setColor(ui::ColorId::DisabledText);
        message->setLayoutHints({.fillWidth = true});

        passphrase = addChild<ui::TextField>();
        passphrase->setName("unlock:passphrase");
        passphrase->setPlaceholder(tr("Passphrase"));
        passphrase->setEchoMode(ui::TextField::EchoMode::Password);
        passphrase->setLayoutHints({.fillWidth = true});
        passphrase->onAccepted = [this] { accept(); };
        passphrase->onTextChanged = [this] { sync(); };

        m_status = addChild<StatusLine>();
        m_status->setLayoutHints({.fillWidth = true});

        auto* actions = addChild<ui::RowLayout>();
        actions->setSpacing(8);
        ui::LayoutHints actionHints {.horizontalAlignment = ui::Alignment::End};
        actionHints.margins.top = 4;
        actions->setLayoutHints(actionHints);
        m_cancel = actions->addChild<ui::DialogButton>(tr("Cancel"));
        m_cancel->onClicked = [this] { m_dialog->m_dialog->close(); };
        m_unlock = actions->addChild<ui::DialogButton>(tr("Unlock"), true);
        m_unlock->setName("unlock:unlock");
        m_unlock->onClicked = [this] { accept(); };
        sync();
    }

    void accept()
    {
        if (m_dialog->m_waiting || passphrase->text().empty())
            return;
        m_dialog->m_waiting = true;
        m_dialog->m_failed = false;
        sync();
        // May finish before it returns.
        m_dialog->m_places.answerPassphrase(m_dialog->m_udi, passphrase->text());
    }

    // Shows the dialog's state: unlocking, that the last try failed, or
    // neither.
    void sync()
    {
        const bool waiting = m_dialog->m_waiting;
        passphrase->setEnabled(!waiting);
        m_cancel->setEnabled(!waiting);
        m_unlock->setEnabled(!waiting && !passphrase->text().empty());

        std::string status;
        if (waiting)
            status = tr("Unlocking…");
        else if (m_dialog->m_failed)
            status = tr("Unlock failed.");
        m_status->text->setText(status);
        m_status->text->setColor(waiting ? ui::ColorId::DisabledText : ui::ColorId::DestructiveAction);
        m_status->busy->setVisible(waiting);
        m_status->busy->setRunning(waiting);
        m_status->setVisible(!status.empty());
        m_status->requestLayout();
        requestLayout();
    }

    ui::TextField* passphrase;

private:
    UnlockVolumeDialog* m_dialog;
    StatusLine* m_status;
    ui::DialogButton* m_cancel;
    ui::DialogButton* m_unlock;
};

UnlockVolumeDialog::UnlockVolumeDialog(ui::Window* parent, PlacesModel& places, std::string udi, std::string name)
    : m_places(places)
    , m_udi(std::move(udi))
    , m_name(std::move(name))
{
    m_dialog = std::make_unique<ui::DialogWindow>(parent, tr("Unlock Volume"), 400);
    // Cancel closes it.
    m_dialog->setCloseVisible(false);
    m_content = m_dialog->setContent<Content>(this);
    m_dialog->onClosed = [this] {
        if (onDone)
            onDone();
    };
    m_dialog->window()->setBackgroundColor(ui::ColorId::SecondBackground);
    m_dialog->window()->addShortcut(ui::NoModifier, ui::key::Escape, [this] {
        // An unlock already sent cannot be taken back; its outcome is shown here.
        if (!m_waiting)
            m_dialog->close();
    });
    m_content->passphrase->setFocus();

    m_subscriptions.push_back(m_places.unlocked.connect([this](const std::string& udi) {
        if (udi == m_udi && m_waiting)
            finished(true, false);
    }));
    m_subscriptions.push_back(
            m_places.unlockFailed.connect([this](const std::string& udi, bool wrong) {
                if (udi == m_udi && m_waiting)
                    finished(false, wrong);
            }));
}

UnlockVolumeDialog::~UnlockVolumeDialog() = default;

void UnlockVolumeDialog::show()
{
    m_dialog->show();
}

void UnlockVolumeDialog::finished(bool ok, bool wrongPassphrase)
{
    m_waiting = false;
    // Closed: it goes once the event is over.
    if (!m_dialog->window())
        return;
    if (ok) {
        // The passphrase stays in memory no longer than it is needed.
        m_content->passphrase->setText({});
        m_dialog->close();
        return;
    }
    m_failed = true;
    if (wrongPassphrase)
        m_content->passphrase->setText({});
    m_content->sync();
    m_content->passphrase->setFocus();
}

void UnlockVolumeDialog::retranslate()
{
    // Closed: it goes once the event is over.
    if (!m_dialog->window())
        return;
    const std::string passphrase = m_content->passphrase->text();
    m_dialog->setTitle(tr("Unlock Volume"));
    m_content = m_dialog->setContent<Content>(this);
    m_content->passphrase->setText(passphrase);
    m_content->sync();
    m_content->passphrase->setFocus();
}

} // namespace fm
