// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "testutil.h"

#include "model/foldermodel.h"
#include "views/gridview.h"
#include "views/previewclient.h"

#include "base/dbus.h"
#include "base/url.h"
#include "ui/application.h"

#include <gtest/gtest.h>

using namespace cutefish;
using fmtest::runUntil;

namespace {

// org.cutefish.FilePreview as the views see it, which keeps what they send.
class PreviewService
{
public:
    struct Call
    {
        std::string method;
        std::string snapshot;
    };

    explicit PreviewService(base::dbus::Connection* bus)
        : m_object(bus, "/org/cutefish/FilePreview", "org.cutefish.FilePreview")
    {
        for (const char* method : {"Toggle", "Activate", "Update"}) {
            m_object.addMethod(method, "ss", "", [this, method](base::dbus::MethodCall& call) {
                std::string source, snapshot;
                call.read(source, snapshot);
                calls.push_back({method, snapshot});
                call.reply();
            });
        }
        m_object.addMethod("Release", "s", "", [](base::dbus::MethodCall& call) { call.reply(); });
        m_object.addMethod("IsVisible", "", "b", [this](base::dbus::MethodCall& call) { call.reply(m_visible); });
        m_object.addSignal("VisibilityChanged", "b");
        m_object.addSignal("Navigate", "sii");
        m_object.addSignal("RestoreFocus", "s");
        m_object.publish();
        bus->requestName("org.cutefish.FilePreview");
    }

    void setVisible(bool visible)
    {
        m_visible = visible;
        m_object.emitSignal("VisibilityChanged", visible);
    }

    // Whether a call after the first `from` is `method` and names the file.
    bool received(size_t from, const std::string& method, const std::string& file) const
    {
        for (size_t i = from; i < calls.size(); ++i) {
            if (calls[i].method == method && calls[i].snapshot.find(file) != std::string::npos)
                return true;
        }
        return false;
    }

    std::vector<Call> calls;

private:
    base::dbus::Object m_object;
    bool m_visible = false;
};

} // namespace

extern ui::Application* s_app;

TEST(PreviewClientTest, SelectionIsSentOnlyWhileThePreviewShows)
{
    base::EventLoop& loop = s_app->loop();
    auto bus = base::dbus::Connection::open(base::dbus::Connection::Bus::Session, &loop);
    ASSERT_NE(bus, nullptr);
    PreviewService service(bus.get());
    fmtest::TemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(fmtest::writeFile(directory.filePath("first.txt")));
    ASSERT_TRUE(fmtest::writeFile(directory.filePath("second.txt")));

    fm::FolderModel model;
    model.setUrl(base::url::fromLocalFile(directory.path()));
    ASSERT_TRUE(runUntil(loop, [&] { return model.status() == fm::FolderModel::Status::Ready && model.count() == 2; }));
    fm::FolderGridView view(model);
    fm::PreviewClient client(base::dbus::Connection::session(&loop));
    client.attach(&model, &view);
    const int first = model.rowForUrl(base::url::fromLocalFile(directory.filePath("first.txt")));
    const int second = model.rowForUrl(base::url::fromLocalFile(directory.filePath("second.txt")));
    ASSERT_GE(first, 0);
    ASSERT_GE(second, 0);
    // The client asks whether the preview shows once it sees the service.
    fmtest::runFor(loop, 200);

    model.setSelected(first);
    fmtest::runFor(loop, 200);
    EXPECT_TRUE(service.calls.empty());

    // What was selected meanwhile is sent when the preview appears.
    service.setVisible(true);
    EXPECT_TRUE(runUntil(loop, [&] { return service.received(0, "Update", "first.txt"); }));

    size_t seen = service.calls.size();
    model.clearSelection();
    model.setSelected(second);
    EXPECT_TRUE(runUntil(loop, [&] { return service.received(seen, "Update", "second.txt"); }));

    service.setVisible(false);
    fmtest::runFor(loop, 200);
    seen = service.calls.size();
    model.clearSelection();
    model.setSelected(first);
    fmtest::runFor(loop, 200);
    EXPECT_EQ(service.calls.size(), seen);
}

TEST(PreviewClientTest, ViewAttachedUnderAShownPreviewSendsItsSelection)
{
    base::EventLoop& loop = s_app->loop();
    auto bus = base::dbus::Connection::open(base::dbus::Connection::Bus::Session, &loop);
    ASSERT_NE(bus, nullptr);
    PreviewService service(bus.get());
    service.setVisible(true);
    fmtest::TemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(fmtest::writeFile(directory.filePath("only.txt")));

    fm::FolderModel model;
    model.setUrl(base::url::fromLocalFile(directory.path()));
    ASSERT_TRUE(runUntil(loop, [&] { return model.status() == fm::FolderModel::Status::Ready && model.count() == 1; }));
    fm::FolderGridView view(model);
    fm::PreviewClient client(base::dbus::Connection::session(&loop));
    client.attach(&model, &view);

    model.setSelected(0);
    EXPECT_TRUE(runUntil(loop, [&] { return service.received(0, "Update", "only.txt"); }));
}
