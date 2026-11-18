// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "preview/fontpreview.h"
#include "core/standardfolders.h"

#include "base/paths.h"
#include "base/strings.h"
#include "ui/font_file.h"

#include <filesystem>

namespace fm {

static constexpr size_t kScriptLines = 3;
static constexpr int kSymbolsPerLine = 18;
static constexpr int kSymbolLines = 3;

// A run of each script's letters, the scripts a font is most likely made for
// first.
static constexpr struct
{
    char32_t first;
    char32_t last;
    int count;
} kScripts[] = {
    {0x4E00, 0x9FFF, 14}, // Han
    {0x3042, 0x3093, 16}, // Hiragana
    {0xAC00, 0xD7A3, 14}, // Hangul
    {0x0621, 0x064A, 22}, // Arabic
    {0x05D0, 0x05EA, 22}, // Hebrew
    {0x0905, 0x0939, 20}, // Devanagari
    {0x0E01, 0x0E2E, 20}, // Thai
    {0x0410, 0x044F, 24}, // Cyrillic
    {0x0391, 0x03C9, 24}, // Greek
};

static std::vector<std::string> sampleLines(const ui::FontFace& face)
{
    std::vector<std::string> lines;
    for (const char* line : {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz", "1234567890"}) {
        if (face.covers(line))
            lines.emplace_back(line);
    }
    size_t scripts = 0;
    for (const auto& script : kScripts) {
        if (scripts == kScriptLines)
            break;
        const std::string line = face.characters(script.first, script.last, script.count);
        if (int(base::strings::utf8ToUtf32(line).size()) < script.count)
            continue;
        lines.push_back(line);
        ++scripts;
    }
    if (!lines.empty())
        return lines;

    // A symbol font: the first characters it has.
    const std::u32string symbols =
            base::strings::utf8ToUtf32(face.characters(0x21, 0x10FFFF, kSymbolsPerLine * kSymbolLines));
    for (size_t start = 0; start < symbols.size(); start += kSymbolsPerLine)
        lines.push_back(base::strings::utf32ToUtf8(symbols.substr(start, kSymbolsPerLine)));
    return lines;
}

static bool isUnder(const std::string& path, const std::string& directory)
{
    return path.size() > directory.size() + 1 && path.starts_with(directory) && path[directory.size()] == '/';
}

bool isFontType(const std::string& mimeType)
{
    return mimeType == "font/ttf" || mimeType == "font/otf" || mimeType == "font/collection";
}

std::string userFontDirectory()
{
    return base::paths::dataHome() + "/fonts";
}

FontPreview describeFont(const std::string& path)
{
    FontPreview preview;
    const std::vector<ui::FontFace> faces = ui::readFontFaces(path);
    if (faces.empty())
        return preview;
    for (const ui::FontFace& face : faces)
        preview.faces.push_back({face.index, face.family, face.style, sampleLines(face)});
    preview.defaultFace = ui::defaultFontFace(faces);

    const std::string legacyDirectory = base::paths::home() + "/.fonts";
    const std::vector<std::string> files = ui::installedFontFiles(faces.front());
    for (const std::string& file : files) {
        if (!isUnder(file, userFontDirectory()) && !isUnder(file, legacyDirectory))
            continue;
        // The file shown before another copy of it.
        if (preview.installed.empty() || file == path)
            preview.installed = file;
        preview.state = FontPreview::State::Installed;
    }
    if (preview.state == FontPreview::State::NotInstalled && !files.empty())
        preview.state = FontPreview::State::System;
    return preview;
}

std::string installFont(const std::string& path, const std::string& directory)
{
    namespace fs = std::filesystem;
    std::error_code error;
    fs::create_directories(directory, error);
    const std::string target = directory + '/' + fileName(path);
    // Font directories are scanned at any time; a name with a leading dot is left out.
    const std::string partial = directory + "/." + fileName(path) + ".part";
    fs::copy_file(path, partial, fs::copy_options::overwrite_existing, error);
    if (error)
        return {};
    fs::rename(partial, target, error);
    if (error) {
        fs::remove(partial, error);
        return {};
    }
    return target;
}

} // namespace fm
