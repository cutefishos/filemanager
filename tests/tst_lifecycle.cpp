#include "application.h"
#include "qmltypes.h"

#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char **argv)
{
    QTemporaryDir config;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    CutefishFM::registerQmlTypes();
    CutefishFM::initResources();
    Application app(argc, argv);
    QWindow helper;
    helper.show();
    bool closedLastWindow = false;

    auto browserWindows = [] {
        QList<QWindow *> windows;
        for (auto *window : QGuiApplication::topLevelWindows()) {
            if (window->isVisible() && window->property("tabs").isValid())
                windows.append(window);
        }
        return windows;
    };

    QTimer::singleShot(0, &app, [&] {
        app.openFiles({config.path(), config.path()});
        const auto windows = browserWindows();
        if (windows.size() != 2) {
            app.exit(2);
            return;
        }
        windows.first()->close();
        QTimer::singleShot(100, &app, [&] {
            const auto remaining = browserWindows();
            if (remaining.size() != 1) {
                app.exit(3);
                return;
            }
            closedLastWindow = true;
            remaining.first()->close();
        });
    });
    QTimer::singleShot(5000, &app, [&] { app.exit(4); });
    const int result = app.exec();
    return closedLastWindow ? result : 5;
}
