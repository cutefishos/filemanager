// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "dialogs/progressrow.h"

#include "base/i18n.h"
#include "ui/header_button.h"
#include "ui/progress_bar.h"
#include "ui/theme.h"

#include <cmath>
#include <string>

using cutefish::i18n::tr;

namespace fm {

static constexpr double kStopButton = 24;
static constexpr double kStopGlyph = 16;
// How far the stop glyph's drawing stops short of its box's end, as a share
// of the box.
static constexpr double kStopGlyphEndInset = 3.0 / 56;

// The stop glyph in the secondary text colour, which follows the theme.
class StopButton : public ui::HeaderButton
{
public:
    StopButton()
        : HeaderButton(std::string(), kStopButton)
    {
        setGlyph("stop", kStopGlyph);
    }

protected:
    void layout() override
    {
        setGlyphColor(ui::Theme::current().disabledText);
        HeaderButton::layout();
    }
};

ProgressRow::ProgressRow()
{
    setSpacing(8);
    m_progress = addChild<ui::ProgressBar>();
    m_progress->setLayoutHints({.fillWidth = true, .alignment = ui::Alignment::Center});
    m_stop = addChild<StopButton>();
    m_stop->setToolTip(tr("Stop"));
    m_stop->setLayoutHints({.alignment = ui::Alignment::Center});
    // The button reaches past the bar's line so the lines around it keep
    // their places.
    const double overhang = (kStopButton - m_progress->sizeHint().height) / 2;
    setPadding({.top = -overhang,
                .right = -std::round(kStopGlyphEndInset * kStopGlyph + (kStopButton - kStopGlyph) / 2),
                .bottom = -overhang});
}

} // namespace fm
