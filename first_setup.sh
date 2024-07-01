#!/bin/sh
(cd "$(dirname "$0")"
mkdir -p builddir
mkdir -p subprojects

meson wrap install liburing
meson setup --reconfigure builddir
meson compile -C builddir
)
