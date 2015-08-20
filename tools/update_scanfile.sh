#!/bin/sh
set -eu

mkdir -p kaffeine_build
cd kaffeine_build
cd ..
cd ..
cmake -DBUILD_TOOLS=1
make convertscanfiles
tools/convertscanfiles ./tools/dvb ../kaffeine/src/scanfile.dvb