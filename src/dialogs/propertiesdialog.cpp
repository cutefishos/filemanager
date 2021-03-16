#include "propertiesdialog.h"
#include "../iconthemeprovider.h"

#include <QDir>
#include <QFileInfo>

#include <QQmlApplicationEngine>
#include <QQuickView>
#include <QQmlContext>
#include <QDebug>

PropertiesDialog::PropertiesDialog(const KFileItem &item, QObject *parent)
    : QObject(parent)
{
    m_items.append(item);
    init();
}

PropertiesDialog::PropertiesDialog(const KFileItemList &items, QObject *parent)
    : QObject(parent)
{
    m_items = items;
    init();
}

PropertiesDialog::PropertiesDialog(const QUrl &url, QObject *parent)
    : QObject(parent)
{
    m_items.append(KFileItem(url));
    init();
}

PropertiesDialog::~PropertiesDialog()
{
    if (m_dirSizeJob)
        m_dirSizeJob->kill();
}

void PropertiesDialog::showDialog(const KFileItem &item)
{
    PropertiesDialog *dlg = new PropertiesDialog(item);
    QQmlApplicationEngine *engine = new QQmlApplicationEngine;
    engine->addImageProvider(QStringLiteral("icontheme"), new IconThemeProvider());
    engine->rootContext()->setContextProperty("main", dlg);
    engine->load(QUrl("qrc:/qml/Dialogs/PropertiesDialog.qml"));
}

void PropertiesDialog::showDialog(const KFileItemList &items)
{    
    PropertiesDialog *dlg = new PropertiesDialog(items);
    QQmlApplicationEngine *engine = new QQmlApplicationEngine;
    engine->addImageProvider(QStringLiteral("icontheme"), new IconThemeProvider());
    engine->rootContext()->setContextProperty("main", dlg);
    engine->load(QUrl("qrc:/qml/Dialogs/PropertiesDialog.qml"));
}

bool PropertiesDialog::multiple() const
{
    return m_multiple;
}

QString PropertiesDialog::location() const
{
    return m_location;
}

QString PropertiesDialog::fileName() const
{
    return m_fileName;
}

QString PropertiesDialog::iconName() const
{
    return m_iconName;
}

QString PropertiesDialog::mimeType() const
{
    return m_mimeType;
}

QString PropertiesDialog::size() const
{
    return m_size;
}

QString PropertiesDialog::creationTime() const
{
    return m_creationTime;
}

QString PropertiesDialog::modifiedTime() const
{
    return m_modifiedTime;
}

QString PropertiesDialog::accessedTime() const
{
    return m_accessedTime;
}

void PropertiesDialog::init()
{
    m_multiple = m_items.count() > 1;

    m_dirSizeJob = KIO::directorySize(m_items);

    connect(m_dirSizeJob, &KIO::DirectorySizeJob::result, this, &PropertiesDialog::slotDirSizeFinished);

    if (!m_multiple) {
        KFileItem item = m_items.first();

        QString path;
        m_fileName = m_items.first().name();

        if (item.isDir())
            m_iconName = "folder";
        else
            m_iconName = m_items.first().iconName();

        m_mimeType = m_items.first().mimetype();
        m_size = KIO::convertSize(m_items.first().size());
        m_location = QFileInfo(m_items.first().localPath()).dir().path();

        qDebug() << m_items.first().mimetype() << " ???";

        m_creationTime = item.time(KFileItem::CreationTime).toString();
        m_modifiedTime = item.time(KFileItem::ModificationTime).toString();
        m_accessedTime = item.time(KFileItem::AccessTime).toString();
    } else {
        m_fileName = QString("%1 files").arg(m_items.count());
        m_location = QFileInfo(m_items.first().localPath()).dir().path();
    }
}

void PropertiesDialog::slotDirSizeFinished(KJob *job)
{
    if (job->error())
        return;

    m_size = KIO::convertSize(m_dirSizeJob->totalSize());

    m_dirSizeJob = 0;

    emit sizeChanged();
}
