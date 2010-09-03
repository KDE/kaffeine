#!/bin/sh
set -eu

mkdir -p kaffeine_build
cd kaffeine_build
cmake ../kaffeine -DBUILD_TOOLS=1
make convertscanfiles
tools/convertscanfiles ../dvb-apps/util/scan ../kaffeine/src/scanfile.dvb
