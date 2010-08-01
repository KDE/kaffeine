#!/bin/sh
set -eu

mkdir -p kaffeine_build
cd kaffeine_build
cmake ../kaffeine -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TOOLS=1
make convertscanfiles
tools/convertscanfiles /home/lxuser/Entwicklung/dvb-apps/util/scan /home/lxuser/Entwicklung/kaffeine/src/scanfile.dvb
