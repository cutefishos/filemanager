// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fusemount.h"

#define FUSE_USE_VERSION 31
#include <fuse.h>

#include <QDir>
#include <QFile>
#include <QProcess>

#include <cstring>

static FuseMount *current()
{
    return static_cast<FuseMount *>(fuse_get_context()->private_data);
}

// Runs one backend call with the mount's lock held.
template <class F> static int call(F function)
{
    FuseMount *self = current();
    std::lock_guard guard(self->lock());
    return function(self->backend());
}

static void *fuseInit(struct fuse_conn_info *connection, struct fuse_config *config)
{
    // Larger requests let a backend keep more of each transfer in flight.
    connection->max_write = 1024 * 1024;
    connection->max_readahead = 1024 * 1024;

    // The far end changes under us, so the kernel keeps what it learnt only briefly.
    config->entry_timeout = 1;
    config->attr_timeout = 1;
    config->negative_timeout = 0;
    config->kernel_cache = 0;
    config->use_ino = 0;
    return fuse_get_context()->private_data;
}

static int fuseGetattr(const char *path, struct stat *st, struct fuse_file_info *)
{
    return call([&](Backend *backend) {
        return backend->stat(path, st);
    });
}

static int fuseReaddir(const char *path, void *buffer, fuse_fill_dir_t fill, off_t,
                       struct fuse_file_info *, enum fuse_readdir_flags flags)
{
    std::vector<DirectoryEntry> entries;
    const int result = call([&](Backend *backend) {
        return backend->list(path, &entries);
    });
    if (result < 0)
        return result;

    const auto fillFlags = (flags & FUSE_READDIR_PLUS) ? FUSE_FILL_DIR_PLUS
                                                       : fuse_fill_dir_flags(0);
    fill(buffer, ".", nullptr, 0, fuse_fill_dir_flags(0));
    fill(buffer, "..", nullptr, 0, fuse_fill_dir_flags(0));
    for (const DirectoryEntry &entry : entries) {
        if (fill(buffer, entry.name.c_str(), &entry.st, 0, fillFlags))
            break;
    }

    return 0;
}

static int fuseOpen(const char *path, struct fuse_file_info *info)
{
    return call([&](Backend *backend) {
        return backend->open(path, info->flags, 0, &info->fh);
    });
}

static int fuseCreate(const char *path, mode_t mode, struct fuse_file_info *info)
{
    return call([&](Backend *backend) {
        return backend->open(path, info->flags | O_CREAT, mode, &info->fh);
    });
}

static int fuseRead(const char *, char *buffer, size_t size, off_t offset,
                    struct fuse_file_info *info)
{
    return call([&](Backend *backend) {
        return backend->read(info->fh, buffer, size, offset);
    });
}

static int fuseWrite(const char *, const char *buffer, size_t size, off_t offset,
                     struct fuse_file_info *info)
{
    return call([&](Backend *backend) {
        return backend->write(info->fh, buffer, size, offset);
    });
}

static int fuseFlush(const char *, struct fuse_file_info *info)
{
    return call([&](Backend *backend) {
        return backend->flush(info->fh);
    });
}

static int fuseFsync(const char *, int, struct fuse_file_info *info)
{
    return call([&](Backend *backend) {
        return backend->flush(info->fh);
    });
}

static int fuseRelease(const char *, struct fuse_file_info *info)
{
    return call([&](Backend *backend) {
        return backend->release(info->fh);
    });
}

static int fuseTruncate(const char *path, off_t size, struct fuse_file_info *info)
{
    return call([&](Backend *backend) {
        return backend->truncate(path, size, info ? &info->fh : nullptr);
    });
}

static int fuseUnlink(const char *path)
{
    return call([&](Backend *backend) {
        return backend->remove(path);
    });
}

static int fuseMkdir(const char *path, mode_t mode)
{
    return call([&](Backend *backend) {
        return backend->makeDirectory(path, mode);
    });
}

static int fuseRmdir(const char *path)
{
    return call([&](Backend *backend) {
        return backend->removeDirectory(path);
    });
}

static int fuseRename(const char *from, const char *to, unsigned int flags)
{
    return call([&](Backend *backend) {
        return backend->rename(from, to, flags);
    });
}

static int fuseChmod(const char *path, mode_t mode, struct fuse_file_info *)
{
    return call([&](Backend *backend) {
        return backend->setMode(path, mode);
    });
}

static int fuseUtimens(const char *path, const struct timespec times[2], struct fuse_file_info *)
{
    return call([&](Backend *backend) {
        return backend->setTimes(path, times);
    });
}

static int fuseReadlink(const char *path, char *buffer, size_t size)
{
    std::string target;
    const int result = call([&](Backend *backend) {
        return backend->readLink(path, &target);
    });
    if (result < 0)
        return result;
    if (size == 0)
        return -EINVAL;

    const size_t length = std::min(target.size(), size - 1);
    std::memcpy(buffer, target.data(), length);
    buffer[length] = '\0';
    return 0;
}

static int fuseSymlink(const char *target, const char *path)
{
    return call([&](Backend *backend) {
        return backend->makeLink(target, path);
    });
}

static int fuseStatfs(const char *path, struct statvfs *st)
{
    return call([&](Backend *backend) {
        return backend->statfs(path, st);
    });
}

static const struct fuse_operations *operations()
{
    static const struct fuse_operations ops = [] {
        struct fuse_operations o = {};
        o.init = fuseInit;
        o.getattr = fuseGetattr;
        o.readdir = fuseReaddir;
        o.open = fuseOpen;
        o.create = fuseCreate;
        o.read = fuseRead;
        o.write = fuseWrite;
        o.flush = fuseFlush;
        o.fsync = fuseFsync;
        o.release = fuseRelease;
        o.truncate = fuseTruncate;
        o.unlink = fuseUnlink;
        o.mkdir = fuseMkdir;
        o.rmdir = fuseRmdir;
        o.rename = fuseRename;
        o.chmod = fuseChmod;
        o.utimens = fuseUtimens;
        o.readlink = fuseReadlink;
        o.symlink = fuseSymlink;
        o.statfs = fuseStatfs;
        return o;
    }();

    return &ops;
}

FuseMount::FuseMount(std::unique_ptr<Backend> backend, const QString &mountPoint,
                     const QString &source, std::function<void(FuseMount *)> finished)
    : m_backend(std::move(backend))
    , m_mountPoint(mountPoint)
    , m_source(source)
    , m_onFinished(std::move(finished))
{
}

FuseMount::~FuseMount()
{
    if (m_thread.joinable())
        m_thread.join();

    if (m_fuse) {
        fuse_unmount(m_fuse);
        fuse_destroy(m_fuse);
    }

    QDir().rmdir(m_mountPoint);
}

// Commas separate mount options, so the source shown in the mount table escapes them.
static QByteArray optionValue(const QString &value)
{
    QByteArray result = value.toUtf8();
    result.replace("\\", "\\\\");
    result.replace(",", "\\,");
    return result;
}

bool FuseMount::start(QString *message)
{
    if (!QDir().mkpath(m_mountPoint)) {
        *message = QStringLiteral("Cannot create %1").arg(m_mountPoint);
        return false;
    }

    const QByteArray options = "fsname=" + optionValue(m_source) + ",subtype=cutefish-mountd";
    const char *argv[] = {"cutefish-mountd", "-o", options.constData()};
    struct fuse_args args = FUSE_ARGS_INIT(3, const_cast<char **>(argv));

    m_fuse = fuse_new(&args, operations(), sizeof(struct fuse_operations), this);
    if (!m_fuse) {
        *message = QStringLiteral("Cannot start the FUSE session");
        return false;
    }

    if (fuse_mount(m_fuse, QFile::encodeName(m_mountPoint).constData()) != 0) {
        fuse_destroy(m_fuse);
        m_fuse = nullptr;
        QDir().rmdir(m_mountPoint);
        *message = QStringLiteral("Cannot mount %1").arg(m_mountPoint);
        return false;
    }

    m_thread = std::thread([this] {
        fuse_loop(m_fuse);
        m_finished = true;
        if (m_onFinished)
            m_onFinished(this);
    });

    return true;
}

bool FuseMount::stop(bool force, QString *message)
{
    QStringList arguments {QStringLiteral("-u")};
    if (force)
        arguments.append(QStringLiteral("-z"));
    arguments.append(m_mountPoint);

    QProcess fusermount;
    fusermount.start(QStringLiteral("fusermount3"), arguments);
    if (!fusermount.waitForFinished(10000) || fusermount.exitCode() != 0) {
        if (message)
            *message = QString::fromLocal8Bit(fusermount.readAllStandardError()).trimmed();
        return false;
    }

    return true;
}

bool FuseMount::finished() const
{
    return m_finished;
}
