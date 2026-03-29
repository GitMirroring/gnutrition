# GNUtrition

GNUtrition is a GNU/Linux nutrition tracker that queries USDA food data
from a local SQLite database. It provides an ncurses-based interactive
interface by default and a command-line interface for searching foods,
logging meals, and reviewing daily food-group budgets.

## Dependencies

### Build-time

- GNU Make
- C compiler with C99 support (e.g., GCC)
- ncurses development headers and library
- sqlite3 development headers and library
- libm (typically part of the C standard library on GNU/Linux)

### Runtime

- ncurses library
- sqlite3 library
- A USDA food database (food.db) built with build_db.sh

### USDA database build requirements

The build_db.sh script downloads USDA data and builds food.db. It
requires the following command-line tools:

- wget
- sha512sum
- libreoffice (for converting Excel files)
- sqlite3 (command-line shell)

## Building GNUtrition

From the top-level directory:

If you're building from a git checkout first run autoreconf -i.
Skip that step if you're building from a release tarball.

Next, run:

./configure

Run --configure --help to see all available options.

Then run: make.

To install system-wide (default prefix is /usr/local):

sudo make install

To change the installation prefix:

make PREFIX=/usr
sudo make PREFIX=/usr install

## Building the USDA database

Run the script in the top-level directory:

./build_db.sh

This downloads USDA data and generates food.db in the current
working directory.

## Running GNUtrition

If food.db is in the current directory:

gnutrition

To specify a different database location:

gnutrition --db=/path/to/food.db

The per-user log database is stored at
$XDG_DATA_HOME/gnutrition/log.db (or ~/.local/share/gnutrition/log.db
if XDG_DATA_HOME is not set). The USDA database is never modified.

## License

Copyright (C) 2026 Free Software Foundation, Inc.

GNUtrition is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.
