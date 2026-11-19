// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "testutil.h"

#include "preview/fontpreview.h"
#include "preview/previewcontroller.h"
#include "preview/previewwindow.h"

#include "base/url.h"
#include "ui/application.h"
#include "ui/combo_box.h"
#include "ui/font_file.h"
#include "ui/image.h"
#include "ui/label.h"
#include "ui/window.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <unicode/ucnv.h>
#include <vector>

using fm::PreviewFile;
using fmtest::TemporaryDir;
using namespace cutefish;

extern ui::Application* s_app;

static PreviewFile describe(const TemporaryDir& directory, const std::string& name, const std::string& contents)
{
    const std::string path = directory.filePath(name);
    fmtest::writeFile(path, contents);
    return fm::describeFile(cutefish::base::url::fromLocalFile(path));
}

static std::string encode(const std::string& utf8, const char* encoding)
{
    UErrorCode status = U_ZERO_ERROR;
    std::string out(utf8.size() * 4 + 16, '\0');
    const int32_t length = ucnv_convert(encoding, "UTF-8", out.data(), int32_t(out.size()), utf8.data(),
                                        int32_t(utf8.size()), &status);
    out.resize(U_SUCCESS(status) ? size_t(length) : 0);
    return out;
}

TEST(PreviewTest, TextIsDecoded)
{
    TemporaryDir directory;
    const std::string sample = "\u7B2C\u4E00\u884C first\n\tsecond\n";
    std::string crlf;
    for (char c : sample)
        crlf += c == '\n' ? std::string("\r\n") : std::string(1, c);
    const struct
    {
        const char* name;
        std::string contents;
        std::string text;
    } cases[] = {
        {"notes.txt", sample, sample},
        {"notes.txt", crlf, sample},
        {"notes.txt", "\xff\xfe" + encode(sample, "UTF-16LE"), sample},
        {"main.cpp", "int main() {}\n", "int main() {}\n"},
        {"README", "Plain words.\n", "Plain words.\n"},
        {"notes.txt", encode("\u8FD9\u662F\u4E00\u4E2A\u7528 GBK \u7F16\u7801\u4FDD\u5B58\u7684\u4E2D\u6587\u6587\u672C\u6587\u4EF6\u3002\n", "GB18030"),
         "\u8FD9\u662F\u4E00\u4E2A\u7528 GBK \u7F16\u7801\u4FDD\u5B58\u7684\u4E2D\u6587\u6587\u672C\u6587\u4EF6\u3002\n"},
        {"notes.txt", encode("\u9019\u662F\u4E00\u500B\u7528\u7E41\u9AD4\u4E2D\u6587\u5BEB\u7684\u6587\u4EF6\uFF0C\u4F7F\u7528 Big5 \u7DE8\u78BC\u4FDD\u5B58\u3002\n", "Big5"),
         "\u9019\u662F\u4E00\u500B\u7528\u7E41\u9AD4\u4E2D\u6587\u5BEB\u7684\u6587\u4EF6\uFF0C\u4F7F\u7528 Big5 \u7DE8\u78BC\u4FDD\u5B58\u3002\n"},
        {"notes.txt", encode("\u3053\u308C\u306F Shift_JIS \u3067\u4FDD\u5B58\u3055\u308C\u305F\u65E5\u672C\u8A9E\u306E\u30C6\u30AD\u30B9\u30C8\u3067\u3059\u3002\n", "Shift_JIS"),
         "\u3053\u308C\u306F Shift_JIS \u3067\u4FDD\u5B58\u3055\u308C\u305F\u65E5\u672C\u8A9E\u306E\u30C6\u30AD\u30B9\u30C8\u3067\u3059\u3002\n"},
        {"notes.txt", encode("\uC774 \uD30C\uC77C\uC740 EUC-KR \uC778\uCF54\uB529\uC73C\uB85C \uC800\uC7A5\uB41C \uD55C\uAD6D\uC5B4 \uD14D\uC2A4\uD2B8\uC785\uB2C8\uB2E4.\n", "EUC-KR"),
         "\uC774 \uD30C\uC77C\uC740 EUC-KR \uC778\uCF54\uB529\uC73C\uB85C \uC800\uC7A5\uB41C \uD55C\uAD6D\uC5B4 \uD14D\uC2A4\uD2B8\uC785\uB2C8\uB2E4.\n"},
        {"notes.txt", encode("Привет, это текст в кодировке Windows-1251.\n", "windows-1251"),
         "Привет, это текст в кодировке Windows-1251.\n"},
        {"notes.txt", encode("Café, naïve façade, résumé.\n", "windows-1252"), "Café, naïve façade, résumé.\n"},
    };
    for (const auto& entry : cases) {
        const PreviewFile file = describe(directory, entry.name, entry.contents);
        EXPECT_EQ(file.kind, PreviewFile::Kind::Text) << entry.text;
        EXPECT_EQ(file.text, entry.text);
        EXPECT_FALSE(file.truncated);
    }
}

TEST(PreviewTest, BinaryContentIsNotText)
{
    TemporaryDir directory;
    const PreviewFile file = describe(directory, "data.txt", std::string("abc\0def\n", 8));
    EXPECT_EQ(file.kind, PreviewFile::Kind::Generic);
    EXPECT_TRUE(file.text.empty());
}

TEST(PreviewTest, LongTextEndsOnAWholeLine)
{
    TemporaryDir directory;
    const std::string line = std::string(99, 'x') + '\n';
    std::string contents;
    for (int i = 0; i < 4000; ++i)
        contents += line;
    const PreviewFile file = describe(directory, "long.txt", contents);
    EXPECT_EQ(file.kind, PreviewFile::Kind::Text);
    EXPECT_TRUE(file.truncated);
    EXPECT_LE(file.text.size(), 256u * 1024);
    EXPECT_GT(file.text.size(), 250u * 1024);
    EXPECT_TRUE(file.text.ends_with(line));
}

TEST(PreviewTest, FoldersCountTheirItems)
{
    TemporaryDir directory;
    fmtest::writeFile(directory.filePath("a"));
    fmtest::writeFile(directory.filePath(".hidden"));
    const PreviewFile file = fm::describeFile(cutefish::base::url::fromLocalFile(directory.path()));
    EXPECT_TRUE(file.folder);
    EXPECT_EQ(file.itemCount, 2u);
    EXPECT_EQ(file.icon, "folder");
}

TEST(PreviewTest, MissingAndRemoteFilesSayWhy)
{
    TemporaryDir directory;
    EXPECT_FALSE(fm::describeFile(cutefish::base::url::fromLocalFile(directory.filePath("gone"))).message.empty());
    EXPECT_FALSE(fm::describeFile("sftp://host/file").message.empty());
}

TEST(PreviewTest, ImagesAreDescribedFromTheirHeaders)
{
    TemporaryDir directory;
    // A 3x2 PNG header; only the IHDR is read.
    const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + std::string("\0\0\0\x0dIHDR", 8)
            + std::string("\0\0\0\x03\0\0\0\x02\x08\x06\0\0\0", 13) + std::string(4, '\0')
            + std::string("\0\0\0\0IEND", 8);
    const PreviewFile file = describe(directory, "a.png", png);
    EXPECT_EQ(file.kind, PreviewFile::Kind::Image);
    EXPECT_EQ(file.width, 3);
    EXPECT_EQ(file.height, 2);
    EXPECT_FALSE(file.animated);
}

static const char kFont[] = "/usr/share/fonts/opentype/inter/Inter-Regular.otf";
static const char kFontCollection[] = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc";
static const char kArabicFont[] = "/usr/share/fonts/truetype/noto/NotoNaskhArabic-Regular.ttf";
static const char kSymbolFont[] = "/usr/share/fonts/truetype/noto/NotoMusic-Regular.ttf";

#define REQUIRE_FONT(path) \
    if (!std::filesystem::exists(path)) \
        GTEST_SKIP() << path << " is not installed"

TEST(PreviewTest, FontsAreDescribedByTheirFaces)
{
    REQUIRE_FONT(kFont);
    TemporaryDir directory;
    const std::string path = directory.filePath("Sample.otf");
    std::filesystem::copy_file(kFont, path);
    const PreviewFile file = fm::describeFile(cutefish::base::url::fromLocalFile(path));
    ASSERT_EQ(file.kind, PreviewFile::Kind::Font);
    ASSERT_EQ(file.font.faces.size(), 1u);
    EXPECT_EQ(file.font.defaultFace, 0);
    const fm::FontPreviewFace& face = file.font.faces.front();
    EXPECT_EQ(face.family, "Inter");
    EXPECT_EQ(face.style, "Regular");
    ASSERT_GE(face.sample.size(), 3u);
    EXPECT_EQ(face.sample[0], "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    EXPECT_EQ(face.sample[1], "abcdefghijklmnopqrstuvwxyz");
    EXPECT_EQ(face.sample[2], "1234567890");
    // The same font is among the system's.
    EXPECT_EQ(file.font.state, fm::FontPreview::State::System);
    EXPECT_TRUE(file.font.installed.empty());
}

TEST(PreviewTest, FontCollectionsListEveryFace)
{
    REQUIRE_FONT(kFontCollection);
    const PreviewFile file = fm::describeFile(cutefish::base::url::fromLocalFile(kFontCollection));
    ASSERT_EQ(file.kind, PreviewFile::Kind::Font);
    ASSERT_GT(file.font.faces.size(), 1u);
    ASSERT_LT(size_t(file.font.defaultFace), file.font.faces.size());
    for (const fm::FontPreviewFace& face : file.font.faces)
        EXPECT_GE(face.sample.size(), 3u);
}

static std::vector<std::string> sampleFor(const char* path, const std::string& language)
{
    const std::vector<cutefish::ui::FontFace> faces = cutefish::ui::readFontFaces(path);
    if (faces.empty())
        return {};
    return fm::fontSampleLines(faces.front(), language);
}

static const std::vector<std::string> kLatinLines = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz",
                                                     "1234567890"};

TEST(PreviewTest, FontSamplesShowOnlyTheReadersLanguageWhenTheFontHasIt)
{
    REQUIRE_FONT(kFont);
    REQUIRE_FONT(kFontCollection);
    // Inter has Cyrillic and Greek too.
    EXPECT_EQ(sampleFor(kFont, "en"), kLatinLines);

    std::vector<std::string> lines = sampleFor(kFontCollection, "zh");
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[3].substr(0, 3), "\u4E00");

    lines = sampleFor(kFontCollection, "ja");
    ASSERT_EQ(lines.size(), 5u);
    EXPECT_EQ(lines[3].substr(0, 3), "\u4E00");
    EXPECT_EQ(lines[4].substr(0, 3), "\u3042");

    lines = sampleFor(kFontCollection, "ru");
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[3].substr(0, 2), "\u0410");
}

TEST(PreviewTest, FontSamplesShowTheFontsScriptsWhenItLacksTheReadersLanguage)
{
    REQUIRE_FONT(kFont);
    REQUIRE_FONT(kArabicFont);
    std::vector<std::string> lines = sampleFor(kFont, "zh");
    ASSERT_EQ(lines.size(), 5u);
    EXPECT_EQ(lines[3].substr(0, 2), "\u0410");
    EXPECT_EQ(lines[4].substr(0, 2), "\u0391");

    lines = sampleFor(kArabicFont, "en");
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines.back().substr(0, 2), "\u0621");
}

TEST(PreviewTest, FontsOfOtherScriptsShowTheirOwnLetters)
{
    REQUIRE_FONT(kArabicFont);
    const PreviewFile file = fm::describeFile(cutefish::base::url::fromLocalFile(kArabicFont));
    ASSERT_EQ(file.kind, PreviewFile::Kind::Font);
    const fm::FontPreviewFace& face = file.font.faces.front();
    ASSERT_FALSE(face.sample.empty());
    EXPECT_NE(face.sample.front(), "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    EXPECT_EQ(face.sample.back().substr(0, 2), "\u0621");
}

TEST(PreviewTest, SymbolFontsShowTheCharactersTheyHave)
{
    REQUIRE_FONT(kSymbolFont);
    const PreviewFile file = fm::describeFile(cutefish::base::url::fromLocalFile(kSymbolFont));
    ASSERT_EQ(file.kind, PreviewFile::Kind::Font);
    const fm::FontPreviewFace& face = file.font.faces.front();
    ASSERT_FALSE(face.sample.empty());
    EXPECT_LE(face.sample.size(), 3u);
    for (const std::string& line : face.sample) {
        EXPECT_FALSE(line.empty());
        EXPECT_EQ(line.find('A'), std::string::npos);
    }
}

TEST(PreviewTest, AFileNamedLikeAFontIsNoFont)
{
    TemporaryDir directory;
    const PreviewFile file = describe(directory, "notes.otf", "Not a font.\n");
    EXPECT_NE(file.kind, PreviewFile::Kind::Font);
    EXPECT_TRUE(file.font.faces.empty());
}

TEST(PreviewTest, InstallingCopiesTheFontIntoTheDirectory)
{
    TemporaryDir directory;
    const std::string source = directory.filePath("Sample.otf");
    fmtest::writeFile(source, "first");
    const std::string fonts = directory.filePath("share/fonts");
    const std::string installed = fm::installFont(source, fonts);
    EXPECT_EQ(installed, fonts + "/Sample.otf");
    EXPECT_EQ(std::filesystem::file_size(installed), 5u);

    // Again, in place of the copy there, and nothing else left behind.
    fmtest::writeFile(source, "second");
    EXPECT_EQ(fm::installFont(source, fonts), installed);
    EXPECT_EQ(std::filesystem::file_size(installed), 6u);
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(fonts), std::filesystem::directory_iterator()), 1);

    EXPECT_TRUE(fm::installFont(directory.filePath("gone.otf"), fonts).empty());
}

template<typename T>
static void collect(ui::View* view, std::vector<T*>& found)
{
    if (!view->isVisible())
        return;
    if (T* match = dynamic_cast<T*>(view))
        found.push_back(match);
    for (const auto& child : view->children())
        collect(child.get(), found);
}

static ui::Label* labelWith(ui::Window* window, const std::string& text)
{
    std::vector<ui::Label*> labels;
    collect(window->contentView(), labels);
    for (ui::Label* label : labels) {
        if (label->text() == text)
            return label;
    }
    return nullptr;
}

static bool sameImage(const ui::Image& a, const ui::Image& b)
{
    if (a.width() != b.width() || a.height() != b.height())
        return false;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            if (a.pixel(x, y).argb() != b.pixel(x, y).argb())
                return false;
        }
    }
    return true;
}

TEST(PreviewTest, AFontCollectionSwitchesBetweenItsFaces)
{
    REQUIRE_FONT(kFontCollection);
    fm::PreviewController controller;
    fm::PreviewWindow preview(controller);
    const auto shown = [&] { return preview.isShown() && !controller.isLoading(); };
    controller.present({{base::url::fromLocalFile(kFontCollection), "NotoSansCJK-Regular.ttc", {}, {}}}, 0, true, 0);
    ASSERT_TRUE(fmtest::runUntil(s_app->loop(), shown));
    const std::vector<fm::FontPreviewFace> faces = controller.file().font.faces;
    ASSERT_GT(faces.size(), 2u);

    preview.window()->grab();
    std::vector<ui::ComboBox*> boxes;
    collect(preview.window()->contentView(), boxes);
    ASSERT_EQ(boxes.size(), 1u);
    ui::ComboBox* styles = boxes.front();
    // The faces are of several families, so each is named in full.
    ASSERT_EQ(styles->items().size(), faces.size());
    EXPECT_EQ(styles->items()[1], faces[1].family + ' ' + faces[1].style);
    EXPECT_EQ(styles->currentIndex(), controller.file().font.defaultFace);
    EXPECT_FALSE(styles->acceptsFocus());
    EXPECT_NE(labelWith(preview.window(), faces[size_t(styles->currentIndex())].family), nullptr);
    const ui::Image first = preview.window()->grab();

    const int other = styles->currentIndex() == 1 ? 2 : 1;
    styles->onActivated(other);
    EXPECT_EQ(styles->currentIndex(), other);
    EXPECT_NE(labelWith(preview.window(), faces[size_t(other)].family), nullptr);
    EXPECT_FALSE(sameImage(first, preview.window()->grab()));

    // Described again, as after Install: the face chosen stays.
    controller.reload();
    ASSERT_TRUE(fmtest::runUntil(s_app->loop(), shown));
    EXPECT_EQ(styles->currentIndex(), other);

    // A font of the system's is neither installed nor removed here.
    ui::Label* installed = labelWith(preview.window(), "Installed");
    ASSERT_NE(installed, nullptr);
    EXPECT_FALSE(installed->parent()->isEnabled());
    controller.close();
}

TEST(PreviewTest, AFontOfOneFaceNamesItsStyle)
{
    REQUIRE_FONT(kFont);
    fm::PreviewController controller;
    fm::PreviewWindow preview(controller);
    controller.present({{base::url::fromLocalFile(kFont), "Inter-Regular.otf", {}, {}}}, 0, true, 0);
    ASSERT_TRUE(fmtest::runUntil(s_app->loop(), [&] { return preview.isShown() && !controller.isLoading(); }));
    preview.window()->grab();
    std::vector<ui::ComboBox*> boxes;
    collect(preview.window()->contentView(), boxes);
    EXPECT_TRUE(boxes.empty());
    EXPECT_NE(labelWith(preview.window(), "Inter"), nullptr);
    EXPECT_NE(labelWith(preview.window(), "Regular"), nullptr);
    controller.close();
}
