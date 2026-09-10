#pragma once
#include <task.h>

class CFileSizeJob : public QObject
{
    Q_OBJECT
public:
    explicit CFileSizeJob(QObject *parent = nullptr);
    ~CFileSizeJob() override;
    qint64 totalSize() const;
    void start(const QList<QUrl> &urls);
    void stop();
signals:
    void sizeChanged();
    void result();

private:
    QPointer<Cutefish::IO::Task> m_task;
    quint64 m_totalSize = 0;
};
