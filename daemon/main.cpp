// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileoperationsservice.h"

#include "base/dbus.h"
#include "base/event_loop.h"

#include <cstdio>

using namespace cutefish;

int main()
{
    base::EventLoop loop;
    base::dbus::Connection *bus = base::dbus::Connection::session(&loop);
    if (!bus) {
        std::fprintf(stderr, "Cannot connect to the session D-Bus.\n");
        return 1;
    }

    FileOperationsService service(bus);
    if (!service.publish() || !bus->requestName("com.cutefish.FileOperations")) {
        std::fprintf(stderr, "Cannot own com.cutefish.FileOperations.\n");
        return 1;
    }

    return loop.run();
}
