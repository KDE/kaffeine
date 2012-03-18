#!/bin/sh
set -eu

# not for production use

sourcedir=$(pwd)
cd ~/bin/kaffeine-build
cmake -DBUILD_TOOLS=1 .
make updatesource
cd $sourcedir
~/bin/kaffeine-build/tools/updatesource
