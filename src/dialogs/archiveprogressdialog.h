// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "dialogs/framelessdialog.h"

#include "base/signal.h"

namespace fm {

class ArchivePasswordDialog;
class FolderModel;

// A compression or extraction of the model's,
// over its window while it runs: the file at hand, the progress (or a busy
// indicator while the size is unknown) and Cancel. An extraction waiting for
// a password asks for it in a dialog of its own instead.
class ArchiveProgressDialog : public FramelessDialog
{
public:
    ArchiveProgressDialog();
    ~ArchiveProgressDialog() override;

    void setModel(FolderModel* model, ui::Window* host);

private:
    std::unique_ptr<ui::View> createContent();
    void update();
    void askPassword(uint32_t request);
    void dismissPassword();

    FolderModel* m_model = nullptr;
    ui::Window* m_host = nullptr;
    base::Subscription m_changed;
    class Content;
    Content* m_content = nullptr;
    std::unique_ptr<ArchivePasswordDialog> m_password;
    // The request the dialog asks, or asked last, in the running job.
    uint32_t m_passwordRequest = 0;
};

} // namespace fm
