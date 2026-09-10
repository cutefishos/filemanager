# File Manager

Cutefish File Manager, simple to use, beautiful, and retain the classic PC interactive design. 

![screenshot](screenshots/Screenshot_20211025_151224.png)

## Dependencies

### Ubuntu

```
sudo apt install equivs curl git devscripts lintian build-essential automake autotools-dev --no-install-recommends

sudo mk-build-deps -i -t "apt-get --yes" -r
```

### Debian

```
sudo apt install build-essential cmake extra-cmake-modules libkf6solid-dev libkf6config-dev libkf6xmlgui-dev libzip-dev qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools
```

### ArchLinux

```shell
sudo pacman -S extra-cmake-modules qt6-base qt6-declarative qt6-tools solid kconfig kxmlgui libzip
```

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
