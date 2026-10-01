#include "cutefishapplication.h"
#include "filechooser.h"
#include "qmltypes.h"
#include "waylandparent.h"
#include "windowmanager.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

#include <csignal>
#include <sys/prctl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    const pid_t parent = getppid();
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (getppid() != parent)
        return 1;

    QApplication app(argc, argv);
    Cutefish::Application::prepare();
    app.setApplicationName(QStringLiteral("cutefish-filechooser"));
    // Its own identity, not File Manager's: the dock matches windows by app id,
    // and the dialog must not land on File Manager's task.
    app.setDesktopFileName(QStringLiteral("cutefish-filechooser"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));

    QFile input;
    input.open(stdin, QIODevice::ReadOnly);
    const QByteArray data = input.read(1024 * 1024 + 1);
    const auto document = QJsonDocument::fromJson(data);
    if (data.size() > 1024 * 1024 || !document.isObject())
        return 1;
    const auto request = document.object();
    const QString mode = request.value("mode").toString();
    if (mode != "OpenFile" && mode != "SaveFile" && mode != "SaveFiles")
        return 1;

    CutefishFM::registerQmlTypes();
    CutefishFM::initResources();
    FileChooser chooser(request);
    QQmlApplicationEngine engine;
    // Declared after the engine so its windows are destroyed first.
    WindowManager windows(&engine);
    WindowManager::setDefault(&windows);
    engine.rootContext()->setContextProperty(QStringLiteral("chooser"), &chooser);
    QObject::connect(&chooser, &FileChooser::finished, &app, [](const QJsonObject &result) {
        QFile output;
        output.open(stdout, QIODevice::WriteOnly);
        output.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
        output.write("\n");
        output.flush();
        QCoreApplication::quit();
    });
    engine.load(QUrl(QStringLiteral("qrc:/chooser/FileChooser.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;

    const QString parentWindow = request.value("parent_window").toString();
    const bool modal = request.value("modal").toBool(true);
    // Wayland parenting and modality go through xdg-foreign and xdg_dialog_v1,
    // which Qt does not drive itself; see applyWaylandParent().
    QTimer::singleShot(0, window, [window, parentWindow, modal] {
        window->show();
        applyWaylandParent(window, parentWindow, modal);
        window->requestActivate();
    });
    return app.exec();
}
