// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "fileoperationsservice.h"

#include "base/dbus.h"
#include "base/event_loop.h"
#include "base/i18n.h"

#include <clocale>
#include <cstdio>

using namespace cutefish;

int main()
{
    std::setlocale(LC_ALL, "");
    i18n::setDomain("cutefish-filemanager");
    base::EventLoop loop;
    base::dbus::Connection *bus = base::dbus::Connection::session(&loop);
    if (!bus) {
        std::fprintf(stderr, "Cannot connect to the session D-Bus.\n");
        return 1;
    }

    FileOperationsService service(bus);
    if (!service.publish() || !bus->requestName("org.cutefish.FileOperations")) {
        std::fprintf(stderr, "Cannot own org.cutefish.FileOperations.\n");
        return 1;
    }

    return loop.run();
}
