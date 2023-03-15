# libdbusmenu-lxqt

This library provides a Qt implementation of the DBusMenu protocol.

The DBusMenu protocol makes it possible for applications to export and import
their menus over DBus.

Forked from [desktop-app/libdbusmenu-qt](https://github.com/desktop-app/libdbusmenu-qt)

## Installation

### Compiling source code

Code configuration is handled by CMake. CMake variable `CMAKE_INSTALL_PREFIX`
has to be set to `/usr` on most operating systems. Depending on the way library
paths are dealt with on 64bit systems, variables like `CMAKE_INSTALL_LIBDIR` may
have to be set as well.  

To build run `make`, to install `make install`, which accepts the variable `DESTDIR`
as usual.

### Author

Canonical DX Team
Maintainer: Renato Filho <renato.filho@canonical.com>
Former maintainer: Aurélien Gâteau
