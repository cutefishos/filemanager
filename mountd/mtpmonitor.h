// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QObject>

struct udev;
struct udev_device;
struct udev_monitor;
class QSocketNotifier;
class QTimer;

// USB devices that speak MTP, as udev marks them.
class MtpMonitor : public QObject
{
    Q_OBJECT

public:
    struct Device
    {
        // mtp://<udev ID_SERIAL>, which stays the same when the device is plugged in again.
        QString id;
        QString name;
        uint32_t bus = 0;
        uint8_t number = 0;
    };

    explicit MtpMonitor(QObject *parent = nullptr);
    ~MtpMonitor() override;

    QList<Device> devices() const;
    Device device(const QString &id) const;

signals:
    void added(const QString &id);
    void removed(const QString &id);

private:
    void handle(udev_device *device, bool present);

    struct udev *m_udev = nullptr;
    struct udev_monitor *m_monitor = nullptr;
    QSocketNotifier *m_notifier = nullptr;
    // By sysfs path, which a removal names.
    QHash<QString, Device> m_devices;
    QHash<QString, QTimer *> m_leaving;
};
