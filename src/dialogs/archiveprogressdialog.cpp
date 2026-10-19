// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "dialogs/archiveprogressdialog.h"
#include "dialogs/archivepassworddialog.h"
#include "model/foldermodel.h"

#include "base/event_loop.h"
#include "base/i18n.h"
#include "ui/dialog_window.h"
#include "ui/label.h"
#include "ui/progress_bar.h"
#include "ui/cell_layout.h"
#include "ui/theme.h"

namespace fm {

using ui::rounding::centeredStart;
using cutefish::i18n::tr;

class ArchiveProgressDialog::Content : public ui::View
{
public:
    explicit Content(ArchiveProgressDialog* dialog)
        : m_dialog(dialog)
    {
        file = addChild<ui::Label>();
        file->setElide(ui::Label::Elide::Middle);
        file->setFontSize(14);
        busy = addChild<ui::BusyIndicator>();
        progress = addChild<ui::ProgressBar>();
        cancel = addChild<ui::DialogButton>(tr("Cancel"));
        cancel->onClicked = [this] {
            if (m_dialog->m_model)
                m_dialog->m_model->cancelArchive();
        };
    }

    // The window is 178 tall with the 44 px header; the rest is this.
    double heightForWidth(double) const override { return 178 - 44 - 32; }

    ui::Label* file;
    ui::BusyIndicator* busy;
    ui::ProgressBar* progress;
    ui::DialogButton* cancel;

protected:
    // A ColumnLayout filling the area, spacing 16: the file, a 20 px row,
    // the buttons at the end.
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        file->setColor(ui::Theme::current().disabledText);
        const ui::SizeF buttonSize = cancel->sizeHint();
        const double fileHeight = file->isVisible() ? file->sizeHint().height : 0;
        const int items = file->isVisible() ? 3 : 2;
        const double used = fileHeight + 20 + buttonSize.height + 16 * (items - 1);
        // A filled ColumnLayout spreads what is left over its items.
        const double extra = std::max(0.0, height - used) / items;
        double y = 0;
        if (file->isVisible()) {
            file->setGeometry({0, y + extra / 2, width, fileHeight});
            y += fileHeight + extra + 16;
        }
        const double rowY = y + extra / 2;
        busy->setGeometry({centeredStart(width, 20), rowY, 20, 20});
        progress->setGeometry({0, rowY + centeredStart(20, 6), width, 6});
        y += 20 + extra + 16;
        cancel->setGeometry({width - buttonSize.width, y + extra / 2, buttonSize.width, buttonSize.height});
    }

private:
    ArchiveProgressDialog* m_dialog;
};

ArchiveProgressDialog::ArchiveProgressDialog()
    : FramelessDialog(tr("File operation"), true)
{
    setHeader(44, 8);
    setContentFactory(
            [this] {
                auto content = std::make_unique<Content>(this);
                m_content = content.get();
                return content;
            },
            420, 16);
    // Closing only hides it; the operation goes on.
    onCloseRequested = [this] {
        m_content = nullptr;
        hide();
    };
}

ArchiveProgressDialog::~ArchiveProgressDialog() = default;

void ArchiveProgressDialog::setModel(FolderModel* model, ui::Window* host)
{
    if (model != m_model) {
        dismissPassword();
        m_passwordRequest = 0;
    }
    m_model = model;
    m_host = host;
    m_changed.reset();
    if (model)
        m_changed = model->archiveChanged.connect([this] { update(); });
    update();
}

void ArchiveProgressDialog::retranslate()
{
    setTitle(tr("File operation"));
    update();
    if (m_password)
        m_password->retranslate();
}

void ArchiveProgressDialog::update()
{
    const bool busy = m_model && m_model->archiveBusy();
    // A request stays open while the daemon tries the password given.
    const uint32_t request = busy ? m_model->archivePasswordId() : 0;
    if (request == 0)
        dismissPassword();
    // Each job numbers its requests from 1.
    if (!busy)
        m_passwordRequest = 0;
    if (!busy || request != 0) {
        if (isShown()) {
            m_content = nullptr;
            hide();
        }
        if (request != 0)
            askPassword(request);
        return;
    }
    setHeaderTitle(tr(m_model->archiveOperation().c_str()));
    if (!isShown()) {
        setParentWindow(m_host);
        show();
        if (window())
            window()->activate();
    }
    if (!m_content)
        return;
    const std::string& file = m_model->archiveCurrentFile();
    m_content->file->setText(file);
    m_content->file->setVisible(!file.empty());
    const int progress = m_model->archiveProgress();
    m_content->busy->setVisible(progress < 0);
    m_content->busy->setRunning(progress < 0);
    m_content->progress->setVisible(progress >= 0);
    m_content->progress->setValue(std::max(0, progress) / 100.0);
    m_content->cancel->setText(m_model->archiveCancelling() ? tr("Cancelling…") : tr("Cancel"));
    m_content->cancel->setEnabled(!m_model->archiveCancelling());
    m_content->requestLayout();
}

void ArchiveProgressDialog::askPassword(uint32_t request)
{
    if (request == m_passwordRequest || m_model->archiveCancelling())
        return;
    dismissPassword();
    m_passwordRequest = request;
    m_password = std::make_unique<ArchivePasswordDialog>(m_host, *m_model, request, m_model->archiveSourceName(),
                                                         m_model->archivePasswordRetry());
    ArchivePasswordDialog* raw = m_password.get();
    raw->onDone = [this, raw] {
        if (m_password.get() != raw)
            return;
        // Not from inside the dialog's own callback.
        std::shared_ptr<ArchivePasswordDialog> gone(std::move(m_password));
        base::EventLoop::current()->post([gone] {});
    };
    raw->show();
}

void ArchiveProgressDialog::dismissPassword()
{
    if (m_password)
        m_password->dismiss();
}

} // namespace fm
