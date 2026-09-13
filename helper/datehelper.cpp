#include "datehelper.h"
#include <QDateTime>
#include <QLocale>

DateHelper::DateHelper(QObject *parent)
    : QObject(parent)
{
}

QString DateHelper::friendlyTime(const QDateTime &time)
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 minutes = qRound64(time.secsTo(now) / 60.0f);

    if (minutes < 1)
        return tr("Now");
    else if (minutes == 1)
        return tr("1 minute ago");
    else if (minutes < 60)
        return tr("%1 minutes ago").arg(minutes);

    qint64 hours = qRound64(minutes / 60.0f);
    if (hours == 1)
        return tr("1 hour ago");
    else if (hours < 24)
        return tr("%1 hours ago").arg(hours);

    qint64 days = qRound64(hours / 24.0f);
    if (days == 1)
        return tr("1 day ago");
    else if (days <= 10)
        return tr("%1 days ago").arg(days);

    return QLocale().toString(time, QLocale::ShortFormat);
}

QString DateHelper::columnTime(const QDateTime &time)
{
    if (!time.isValid())
        return QString();

    const QLocale locale;
    const QString clock = locale.toString(time.time(), QLocale::ShortFormat);
    const qint64 days = time.date().daysTo(QDate::currentDate());

    if (days == 0)
        return tr("Today at %1").arg(clock);
    if (days == 1)
        return tr("Yesterday at %1").arg(clock);

    return locale.toString(time, QLocale::ShortFormat);
}
