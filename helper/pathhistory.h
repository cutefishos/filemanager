#ifndef PATHHISTORY_H
#define PATHHISTORY_H

#include <QObject>

class PathHistory : public QObject
{
    Q_OBJECT

public:
    explicit PathHistory(QObject *parent = nullptr);

    void append(const QUrl &path);

    QUrl posteriorPath();
    QUrl previousPath();

private:
    QVector<QUrl> m_prevHistory;
    QVector<QUrl> m_postHistory;
};

#endif // PATHHISTORY_H
