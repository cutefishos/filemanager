# File Manager

Cutefish File Manager, simple to use, beautiful, and retain the classic PC interactive design. 

## Dependencies

### Ubuntu

```
sudo apt install libkf5solid-dev libkf5kio-dev -y
```

### ArchLinux

```shell
sudo pacman -S extra-cmake-modules qt5-base qt5-quickcontrols2 taglib kio
```

## Build

```shell
mkdir build
cd build
cmake ..
make
```

## Install

```shell
sudo make install
```

## License

This project has been licensed by GPLv3.
