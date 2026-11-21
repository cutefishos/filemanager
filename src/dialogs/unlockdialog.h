// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "core/common.h"

#include "base/signal.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cutefish::ui {
class DialogWindow;
class Window;
}

namespace fm {

class PlacesModel;

// The passphrase of an encrypted disk. It stays open while the disk is
// unlocked and mounted, and says there when that failed.
class UnlockVolumeDialog
{
public:
    UnlockVolumeDialog(ui::Window* parent, PlacesModel& places, std::string udi, std::string name);
    ~UnlockVolumeDialog();

    void show();
    // The passphrase typed and the state stay.
    void retranslate();
    std::function<void()> onDone;

private:
    class Content;

    void finished(bool ok, bool wrongPassphrase);

    std::unique_ptr<ui::DialogWindow> m_dialog;
    Content* m_content = nullptr;
    PlacesModel& m_places;
    std::string m_udi;
    std::string m_name;
    bool m_waiting = false;
    bool m_failed = false;
    std::vector<base::Subscription> m_subscriptions;
};

} // namespace fm
