#!/bin/sh
set -eu

mkdir -p kaffeine_build
cd kaffeine_build
cmake ../kaffeine -DBUILD_TOOLS=1
make updatemimetypes
tools/updatemimetypes ../kaffeine/src/kaffeine.desktop ../kaffeine/src/mediawidget.cpp
