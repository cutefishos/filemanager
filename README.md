# File Manager

Cutefish File Manager, simple to use, beautiful, and retain the classic PC interactive design. 

![screenshot](screenshots/Screenshot_20211025_151224.png)

## Dependencies

File Manager uses Qt 6 and Cutefish::IO; it does not depend on KDE Frameworks.

### Debian/Ubuntu

```
sudo apt install build-essential cmake libzip-dev qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools
```

### ArchLinux

```shell
sudo pacman -S qt6-base qt6-declarative qt6-tools libzip
```

## Quick Look

Space toggles a shared preview window for File Manager and the desktop. With no
selection it shows the current folder. Changing selection, folder, tab, or the
focused file view updates that same window; losing focus or closing the source
window does not dismiss it. Space, Escape, Ctrl+W, and its close button dismiss it.
With multiple files selected, Left/Right browse those files without changing the
selection. A compact row shows previous/next arrows and the item position.
Rename and path editing keep
normal Space input.

The window uses FishUI glass, with an opaque fallback on compositors without
the glass protocol. File information has an icon on the left and the filename
and size on the right. Folders show `size, item count` beneath the name; the count
includes immediate hidden children and size is calculated recursively in the
background using the same size job and formatting as Properties. Switching keeps
the current renderer visible until replacement metadata arrives; images retain
their previous contents while decoding the next image. No loading indicator is
shown. Images support Ctrl+Plus/Minus zoom and Ctrl+0 to fit. Text, PDF, and
audio/video contents are not rendered. There is no maximize or fullscreen mode.

`cutefish-filepreview` owns the single window through the session D-Bus service
`com.cutefish.FilePreview`. Install the executable and its activation file along
with File Manager; the desktop plugin uses the same service. The compositor
classifies the installed preview executable into its floating layer, above
applications and below panels and system dialogs. Restart the compositor after
installing this new executable so its process policy can resolve the path.
The shell excludes the preview from the Dock and preserves the source application's
identity and menu in the status bar while the preview has focus.

## Build

```shell
cmake -S . -B /tmp/cutefish-filemanager-build -DCMAKE_INSTALL_PREFIX=/usr -DFILEMANAGER_BUILD_TESTS=ON
cmake --build /tmp/cutefish-filemanager-build
ctest --test-dir /tmp/cutefish-filemanager-build --output-on-failure
```

## Install

```shell
sudo cmake --install /tmp/cutefish-filemanager-build
```

## License

This project has been licensed by GPLv3.

Filesystem operations use [Cutefish::IO](../cutefish-framework/io/README.md), built from the sibling framework checkout.
