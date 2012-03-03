#!/bin/sh
set -eu

# not for production use

sourcedir=$(pwd)
cd ~/bin/kaffeine-build
rm -fr install
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=~/bin/kaffeine-build/install -DSTRICT_BUILD=1 . $sourcedir
make install
