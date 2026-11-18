// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "core/common.h"

#include <string>
#include <vector>

namespace fm {

// One face of a previewed font file.
struct FontPreviewFace
{
    // What ui::FontSample loads.
    int index = 0;
    std::string family;
    std::string style;
    // Lines of characters the face has glyphs for.
    std::vector<std::string> sample;
};

// What Quick Look shows of a font file.
struct FontPreview
{
    enum class State {
        NotInstalled,
        // Among the user's fonts: `installed` is the file to remove.
        Installed,
        // Among the system's fonts, which the user does not change.
        System,
    };

    std::vector<FontPreviewFace> faces;
    // The face shown first.
    int defaultFace = 0;
    State state = State::NotInstalled;
    std::string installed;
};

// The font files Quick Look shows: TrueType and OpenType fonts and collections.
bool isFontType(const std::string& mimeType);
// No faces when the file is not such a font. Any thread.
FontPreview describeFont(const std::string& path);

// ~/.local/share/fonts
std::string userFontDirectory();
// Copies the file into the directory, in place of one of its name there. The
// copy's path, empty when it could not be made. Any thread.
std::string installFont(const std::string& path, const std::string& directory = userFontDirectory());

} // namespace fm
