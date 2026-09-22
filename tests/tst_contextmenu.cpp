#include <QQmlContext>
#include <QQmlEngine>
#include <QCursor>
#include <QtQuickTest/quicktest.h>

class MenuTestSetup : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE void moveCursor(const QPointF &position)
    {
        QCursor::setPos(position.toPoint());
    }

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->rootContext()->setContextProperty("menuTest", this);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(contextmenu, MenuTestSetup)

#include "tst_contextmenu.moc"
