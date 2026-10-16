#ifndef ARCHIVEJOB_H
#define ARCHIVEJOB_H

#include <zip.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

struct archive;

// Compresses files into a ZIP archive, or extracts an archive, on a thread of
// its own. The callbacks run on that thread.
class ArchiveJob
{
public:
    enum Operation {
        Compress,
        Extract,
    };

    ArchiveJob(Operation operation, std::vector<std::string> inputs, std::string outputPath);
    ~ArchiveJob();

    ArchiveJob(const ArchiveJob &) = delete;
    ArchiveJob &operator=(const ArchiveJob &) = delete;

    void start();
    void cancel();

    std::function<void(int progress)> onProgress;
    std::function<void(const std::string &fileName)> onCurrentFile;
    std::function<void(bool success, bool canceled, const std::string &error, const std::string &outputPath)>
            onCompleted;

private:
    struct CompressionEntry {
        std::string sourcePath;
        std::string archivePath;
        bool directory;
        uint64_t size;
        bool symbolicLink;
        std::string linkTarget;
    };

    void run();
    bool compress(std::string *error);
    bool extract(std::string *error);

    bool collectCompressionEntries(const std::string &sourcePath, const std::string &archivePath,
                                   std::vector<CompressionEntry> *entries, std::string *error) const;
    bool archiveEntryPath(const std::string &archiveName, std::string *relativePath, bool *directory) const;
    bool extractEntryData(struct archive *reader, int fd, uint64_t total, std::string *error);
    bool extractSingleFile(struct archive *reader, bool empty, const std::string &parentPath, uint64_t total,
                           std::string *error);

    void emitProgress(uint64_t processed, uint64_t total);
    void progressChanged(int progress);
    void currentFileChanged(const std::string &fileName);
    bool isCanceled() const;

    static void zipProgressCallback(zip_t *archive, double progress, void *state);
    static int zipCancelCallback(zip_t *archive, void *state);

    Operation m_operation;
    std::vector<std::string> m_inputs;
    std::string m_outputPath;
    std::atomic_bool m_cancelRequested;
    int m_lastPercent = -1;
    std::thread m_thread;
};

#endif // ARCHIVEJOB_H
