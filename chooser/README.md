# File Manager chooser

`cutefish-filechooser` provides the FileChooser UI for
`xdg-desktop-portal-cutefish`. It is a separate process built by File Manager and
shares `FolderModel`, `PlacesModel`, icons, and FishUI with the browser. The portal
backend itself has no UI and does not embed the shell or file browser.
The helper uses FishUI controls explicitly. Dialog actions use
`FishUI.ConfirmDialogButton`, and the chooser has a fixed size without maximization.

The dialog has no title bar or window buttons; dragging the path bar moves it and
Escape or `Cancel` closes it. The layout follows the macOS panel: the places
sidebar, a breadcrumb path bar with back/forward, the file list, the `Save As:`
and `File Type:` fields, and an action row with `New Folder` on the left and
`Cancel`/accept on the right. Shortcuts: `Ctrl+L` edits the path,
`Ctrl+Shift+N` creates a folder, `Ctrl+H` toggles hidden files, `Alt+Left`,
`Alt+Right` and `Alt+Up` navigate. The window is opaque; it does not use the
blurred FishUI background.

## Build and test

From the workspace root:

```sh
cmake -S filemanager -B /tmp/cutefish-filechooser-build -G Ninja \
  -DCMAKE_INSTALL_PREFIX=/usr -DFILEMANAGER_BUILD_TESTS=ON
cmake --build /tmp/cutefish-filechooser-build --target cutefish-filechooser tst_filechooser
ctest --test-dir /tmp/cutefish-filechooser-build -R '^filechooser$' --output-on-failure
```

A full File Manager build/install installs the helper under `libexec` alongside
the existing application. The workspace `build.sh` installs File Manager before
the portal, then reloads user units and restarts both portal services after a
successful system install. Staged installs do not restart services.

To include the real UI in the portal's isolated integration tests:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
CUTEFISH_TEST_CHOOSER_HELPER=/tmp/cutefish-filechooser-build/cutefish-filechooser \
ctest --test-dir /tmp/xdg-desktop-portal-cutefish-build --output-on-failure
```

## Private helper protocol

One helper process serves one dialog. It reads one UTF-8 JSON object from stdin,
terminated by EOF, and writes one JSON result to stdout before exiting. Diagnostics
go to stderr. This is a private protocol shared with `xdg-desktop-portal-cutefish`,
not an application-facing API; applications use the standard FileChooser portal.

Input keys:

- `mode`: `OpenFile`, `SaveFile`, or `SaveFiles`.
- `title`, `app_id`, `parent_window`, `modal`, `accept_label`.
- `multiple`, `directory`: open-request selection flags.
- `current_folder`, `current_file`, `current_name`: decoded local paths/names.
- `filters`: `[{"name":"Text","rules":[{"type":0,"value":"*.txt"}]}]`;
  rule type `0` is a glob, `1` a MIME type. Rules within a filter are alternatives.
- `filter_index`: initially selected filter.
- `choices`: `[{"id":"encoding","label":"Encoding","options":[{"id":"utf8","label":"UTF-8"}],"selected":"utf8"}]`.
  Empty `options` describes a checkbox with `true`/`false` string values.
- `files`: suggested basenames for `SaveFiles`.

Successful output contains `response: 0`, `uris` (encoded local file URIs),
`filter_index`, and `choices` (an object mapping choice IDs to selected values).
Cancellation returns `{"response":1}`. A nonzero process exit or malformed result
is a portal failure. Input and output are limited to 1 MiB.

On Wayland the requesting application exports its toplevel through
`xdg-foreign-unstable-v2` and the portal passes `wayland:<handle>`; the helper
imports it, calls `set_parent_of`, and marks itself a modal `xdg_dialog_v1`, so
the dialog stacks above its application and blocks it. X11 parents use
`setTransientParent`. Both need the compositor to provide those globals.

The helper does not create or truncate save targets. Existing save targets require
confirmation; bulk saves choose unused names. Creating a folder is an explicit UI
action. Non-Unicode native paths are not supported yet.

Example preview in a Cutefish session:

```sh
printf '%s\n' '{"mode":"OpenFile","title":"Open File","multiple":true}' \
  | /tmp/cutefish-filechooser-build/cutefish-filechooser
```
