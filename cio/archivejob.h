#ifndef ARCHIVEJOB_H
#define ARCHIVEJOB_H

#include <QThread>
#include <QString>
#include <QStringList>

#include <zip.h>

#include <atomic>

class ArchiveJob : public QThread
{
    Q_OBJECT

public:
    enum Operation {
        Compress,
        Extract,
    };

    ArchiveJob(Operation operation,
               const QStringList &inputs,
               const QString &outputPath,
               QObject *parent = nullptr);
    ~ArchiveJob() override;

    void cancel();

signals:
    void progressChanged(int progress);
    void currentFileChanged(const QString &fileName);
    void completed(bool success,
                   bool canceled,
                   const QString &error,
                   const QString &outputPath);

protected:
    void run() override;

private:
    struct CompressionEntry {
        QString sourcePath;
        QString archivePath;
        bool directory;
        quint64 size;
        bool symbolicLink;
        QString linkTarget;
    };

    bool compress(QString *error);
    bool extract(QString *error);

    bool collectCompressionEntries(const QString &sourcePath,
                                   const QString &archivePath,
                                   QList<CompressionEntry> *entries,
                                   QString *error) const;
    bool archiveEntryPath(const QString &archiveName,
                          QString *relativePath,
                          bool *directory) const;
    static bool zipEntryIsSymbolicLink(zip_t *archive, zip_uint64_t index);

    void emitProgress(quint64 processed, quint64 total);
    bool isCanceled() const;

    static void zipProgressCallback(zip_t *archive, double progress, void *state);
    static int zipCancelCallback(zip_t *archive, void *state);

    Operation m_operation;
    QStringList m_inputs;
    QString m_outputPath;
    std::atomic_bool m_cancelRequested;
};

#endif // ARCHIVEJOB_H
