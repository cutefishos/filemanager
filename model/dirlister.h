// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DIRLISTER_H
#define DIRLISTER_H

#include <directory.h>

class DirLister : public Cutefish::IO::DirectoryLister
{
    Q_OBJECT

public:
    explicit DirLister(QObject *parent = nullptr);
    ~DirLister() override;
};

#endif // DIRLISTER_H
