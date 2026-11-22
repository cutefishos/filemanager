// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "core/common.h"

#include "ui/cell_layout.h"

namespace cutefish::ui {
class HeaderButton;
class ProgressBar;
}

namespace fm {

class ProgressRow : public ui::RowLayout
{
public:
    ProgressRow();

    ui::ProgressBar* progress() const { return m_progress; }
    ui::HeaderButton* stop() const { return m_stop; }

private:
    ui::ProgressBar* m_progress;
    ui::HeaderButton* m_stop;
};

} // namespace fm
