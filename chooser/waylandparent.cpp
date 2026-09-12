#include "waylandparent.h"

#include <QWindow>

#ifdef CUTEFISH_FILECHOOSER_WAYLAND

#include <QGuiApplication>
#include <qguiapplication_platform.h>
#include <qpa/qplatformwindow_p.h>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <xdg-dialog-v1-client-protocol.h>
#include <xdg-foreign-unstable-v2-client-protocol.h>
#include <xdg-shell-client-protocol.h>

#include <cstring>

namespace {

struct Globals {
    zxdg_importer_v2 *importer = nullptr;
    xdg_wm_dialog_v1 *dialogManager = nullptr;
};

void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface,
                    uint32_t version)
{
    auto *globals = static_cast<Globals *>(data);
    if (std::strcmp(interface, zxdg_importer_v2_interface.name) == 0) {
        globals->importer = static_cast<zxdg_importer_v2 *>(
                wl_registry_bind(registry, name, &zxdg_importer_v2_interface, 1));
    } else if (std::strcmp(interface, xdg_wm_dialog_v1_interface.name) == 0) {
        globals->dialogManager = static_cast<xdg_wm_dialog_v1 *>(
                wl_registry_bind(registry, name, &xdg_wm_dialog_v1_interface, qMin(version, 1u)));
    }
}

// Binds on a queue of our own, so this never steals events from Qt's.
const Globals *foreignGlobals(wl_display *display)
{
    static Globals globals;
    static bool bound = false;
    if (bound)
        return &globals;
    bound = true;

    wl_event_queue *queue = wl_display_create_queue(display);
    auto *wrapper = static_cast<wl_display *>(wl_proxy_create_wrapper(display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapper), queue);
    wl_registry *registry = wl_display_get_registry(wrapper);
    wl_proxy_wrapper_destroy(wrapper);

    static const wl_registry_listener listener = {registryGlobal,
                                                  [](void *, wl_registry *, uint32_t) { }};
    wl_registry_add_listener(registry, &listener, &globals);
    wl_display_roundtrip_queue(display, queue);
    return &globals;
}

} // namespace

void applyWaylandParent(QWindow *window, const QString &parentWindow, bool modal)
{
    if (!window || !parentWindow.startsWith(QLatin1String("wayland:")))
        return;
    const QByteArray handle = parentWindow.mid(8).toUtf8();
    if (handle.isEmpty())
        return;

    auto *application = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    auto *native = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    if (!application || !native)
        return;

    wl_surface *surface = native->surface();
    auto *toplevel = native->surfaceRole<::xdg_toplevel>();
    if (!surface || !toplevel) {
        // The window has no surface yet; try again once it has one.
        QObject::connect(
                native, &QNativeInterface::Private::QWaylandWindow::surfaceRoleCreated, window,
                [window, parentWindow, modal] {
                    applyWaylandParent(window, parentWindow, modal);
                },
                Qt::SingleShotConnection);
        return;
    }

    const Globals *globals = foreignGlobals(application->display());
    if (!globals->importer)
        return;

    zxdg_imported_v2 *imported =
            zxdg_importer_v2_import_toplevel(globals->importer, handle.constData());
    zxdg_imported_v2_set_parent_of(imported, surface);

    // The parent relation stacks the dialog; modality is what blocks input to
    // the application behind it.
    if (modal && globals->dialogManager) {
        xdg_dialog_v1 *dialog = xdg_wm_dialog_v1_get_xdg_dialog(globals->dialogManager, toplevel);
        xdg_dialog_v1_set_modal(dialog);
    }
    wl_display_flush(application->display());
}

#else

void applyWaylandParent(QWindow *, const QString &, bool)
{
}

#endif
