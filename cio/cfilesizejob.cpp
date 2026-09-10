#include "cfilesizejob.h"

CFileSizeJob::CFileSizeJob(QObject *parent)
    : QObject(parent)
{
}

CFileSizeJob::~CFileSizeJob()
{
    stop();
}

qint64 CFileSizeJob::totalSize() const
{
    return m_totalSize;
}

void CFileSizeJob::start(const QList<QUrl> &urls)
{
    stop();
    m_totalSize = 0;
    m_task =
            Cutefish::IO::TaskController::instance()->submit({Cutefish::IO::Operation::Size, urls});
    connect(m_task, &Cutefish::IO::Task::progress, this, [this](quint64 bytes, quint64) {
        m_totalSize = bytes;
        emit sizeChanged();
    });
    connect(m_task, &Cutefish::IO::Task::result, this, [this](Cutefish::IO::Task *task) {
        m_totalSize = task->outcome().bytes;
        m_task = nullptr;
        emit result();
    });
}

void CFileSizeJob::stop()
{
    if (!m_task)
        return;
    disconnect(m_task, nullptr, this, nullptr);
    m_task->cancel();
    m_task = nullptr;
}
