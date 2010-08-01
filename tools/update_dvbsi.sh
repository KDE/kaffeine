#!/bin/sh
set -eu

mkdir -p kaffeine_build
cd kaffeine_build
cmake ../kaffeine -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TOOLS=1
make updatedvbsi
cd ../kaffeine/tools
../../kaffeine_build/tools/updatedvbsi
