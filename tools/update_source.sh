#!/bin/sh
set -eu

mkdir -p kaffeine_build
cd kaffeine_build
cmake ../kaffeine -DBUILD_TOOLS=1
make updatesource
cd ../kaffeine
../kaffeine_build/tools/updatesource
