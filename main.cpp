// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "application.h"
#include "qmltypes.h"

int main(int argc, char *argv[])
{
    // Same registration the desktop plugin performs, so both hosts of the
    // shared QML see exactly the same types.
    CutefishFM::registerQmlTypes();

    Application app(argc, argv);
    return app.run();
}
