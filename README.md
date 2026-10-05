# File Manager

Cutefish File Manager, simple to use, beautiful, and retain the classic PC interactive design.

![screenshot](screenshots/Screenshot_20211025_151224.png)

## Dependencies

### Debian/Ubuntu

```shell
sudo apt install build-essential cmake libicu-dev libzip-dev qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools
```

### ArchLinux

```shell
sudo pacman -S qt6-base qt6-declarative qt6-tools libzip icu
```

[Cutefish Framework](https://github.com/cutefishos/cutefish-framework) needs to
be installed first.

## Build

```shell
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

## License

This project has been licensed by GPLv3.
