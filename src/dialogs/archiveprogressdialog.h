// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dialogs/framelessdialog.h"

#include "base/signal.h"

namespace fm {

class FolderModel;

// A compression or extraction of the model's,
// over its window while it runs: the file at hand, the progress (or a busy
// indicator while the size is unknown) and Cancel.
class ArchiveProgressDialog : public FramelessDialog
{
public:
    ArchiveProgressDialog();
    ~ArchiveProgressDialog() override;

    void setModel(FolderModel* model, ui::Window* host);

private:
    std::unique_ptr<ui::View> createContent();
    void update();

    FolderModel* m_model = nullptr;
    ui::Window* m_host = nullptr;
    base::Subscription m_changed;
    class Content;
    Content* m_content = nullptr;
};

} // namespace fm
