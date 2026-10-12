// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mtpmonitor.h"

#include <QSocketNotifier>
#include <QTimer>
#include <QUrl>

#include <libudev.h>

#include <cstring>

// Allowing access on a phone's screen drops it off the bus and brings it back.
static constexpr int kLeaveDelayMs = 5000;

static QString udevProperty(udev_device *device, const char *name)
{
    const char *value = udev_device_get_property_value(device, name);
    return value ? QString::fromUtf8(value) : QString();
}

// udev's *_ENC values escape bytes as \xNN.
static QString decoded(const QString &value)
{
    QByteArray result;
    const QByteArray bytes = value.toUtf8();
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == '\\' && i + 3 < bytes.size() && bytes[i + 1] == 'x') {
            result.append(char(bytes.mid(i + 2, 2).toInt(nullptr, 16)));
            i += 3;
        } else {
            result.append(bytes[i]);
        }
    }
    return QString::fromUtf8(result).trimmed();
}

static QString nameOf(udev_device *device)
{
    const QString player = udevProperty(device, "ID_MEDIA_PLAYER_MODEL");
    if (!player.isEmpty())
        return player;

    if (const char *product = udev_device_get_sysattr_value(device, "product"))
        return QString::fromUtf8(product).trimmed();

    const QString name = decoded(udevProperty(device, "ID_VENDOR_ENC")) + QLatin1Char(' ')
            + decoded(udevProperty(device, "ID_MODEL_ENC"));
    return name.trimmed();
}

MtpMonitor::MtpMonitor(QObject *parent)
    : QObject(parent)
    , m_udev(udev_new())
{
    if (!m_udev)
        return;

    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "usb", "usb_device");
    udev_monitor_enable_receiving(m_monitor);

    m_notifier = new QSocketNotifier(udev_monitor_get_fd(m_monitor), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] {
        while (udev_device *device = udev_monitor_receive_device(m_monitor)) {
            const char *action = udev_device_get_action(device);
            if (action && std::strcmp(action, "remove") == 0)
                handle(device, false);
            else if (action && (std::strcmp(action, "add") == 0
                                || std::strcmp(action, "change") == 0))
                handle(device, true);
            udev_device_unref(device);
        }
    });

    udev_enumerate *enumerate = udev_enumerate_new(m_udev);
    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_add_match_property(enumerate, "ID_MTP_DEVICE", "1");
    udev_enumerate_scan_devices(enumerate);

    udev_list_entry *entry = nullptr;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate))
    {
        if (udev_device *device = udev_device_new_from_syspath(m_udev,
                                                               udev_list_entry_get_name(entry))) {
            handle(device, true);
            udev_device_unref(device);
        }
    }
    udev_enumerate_unref(enumerate);
}

MtpMonitor::~MtpMonitor()
{
    if (m_monitor)
        udev_monitor_unref(m_monitor);
    if (m_udev)
        udev_unref(m_udev);
}

QList<MtpMonitor::Device> MtpMonitor::devices() const
{
    return m_devices.values();
}

MtpMonitor::Device MtpMonitor::device(const QString &id) const
{
    for (const Device &device : m_devices) {
        if (device.id == id)
            return device;
    }
    return {};
}

void MtpMonitor::handle(udev_device *device, bool present)
{
    const QString sysPath = QString::fromUtf8(udev_device_get_syspath(device));

    if (!present) {
        const auto found = m_devices.constFind(sysPath);
        if (found == m_devices.cend())
            return;

        const QString id = found->id;
        m_devices.erase(found);

        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, id, timer] {
            m_leaving.remove(id);
            timer->deleteLater();
            emit removed(id);
        });
        timer->start(kLeaveDelayMs);
        m_leaving.insert(id, timer);
        return;
    }

    // Its interfaces carry the same properties but no bus and device numbers.
    const char *type = udev_device_get_devtype(device);
    if (!type || std::strcmp(type, "usb_device") != 0
        || udevProperty(device, "ID_MTP_DEVICE") != QLatin1String("1"))
        return;

    // The short serial is empty when udev made one up; the full one is always set.
    const QString serial = udevProperty(device, "ID_SERIAL");
    if (serial.isEmpty())
        return;

    Device entry;
    entry.id = QStringLiteral("mtp://") + QString::fromLatin1(QUrl::toPercentEncoding(serial));
    entry.name = nameOf(device);
    entry.bus = udevProperty(device, "BUSNUM").toUInt();
    entry.number = uint8_t(udevProperty(device, "DEVNUM").toUInt());

    // Two devices with one serial cannot be told apart; the first one is kept.
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        if (it->id == entry.id && it.key() != sysPath)
            return;
    }

    const bool returning = m_leaving.contains(entry.id);
    if (QTimer *timer = m_leaving.take(entry.id))
        timer->deleteLater();

    const bool known = m_devices.contains(sysPath);
    m_devices.insert(sysPath, entry);
    if (!known && !returning)
        emit added(entry.id);
}
