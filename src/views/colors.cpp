// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "views/colors.h"

#include "ui/theme.h"

namespace fm {

using namespace cutefish::ui;

void addFmColors(ColorMixer& mixer, const Theme& theme)
{
    const bool dark = theme.darkMode;

    mixer[FmColor::ChooserSidebarBackground] = dark ? lighter(ColorId::Background, 1.35)
                                                    : darker(ColorId::Background, 1.04);
    mixer[FmColor::AlternateRow] = withAlpha(ColorId::Text, dark ? 0.03 : 0.025);
    mixer[FmColor::RubberBandFill] = dark ? Color::rgb(0xffffff).withAlpha(0.12)
                                          : withAlpha(ColorId::DisabledText, 0.08);
    mixer[FmColor::RubberBandBorder] = dark ? Color::rgb(0xffffff).withAlpha(0.7)
                                            : withAlpha(ColorId::DisabledText, 0.5);
    mixer[FmColor::PathEditorText] = dark ? Color::rgb(0xffffff) : Color::rgb(0x000000);
    mixer[FmColor::PathEditorBackground] = dark ? lighter(ColorId::SecondBackground, 1.7)
                                                : ColorRecipe(ColorId::SecondBackground);
    mixer[FmColor::SearchScopeSelected] = withAlpha(ColorId::Highlight, dark ? 0.3 : 0.15);
}

} // namespace fm
