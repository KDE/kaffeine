#!/bin/sh
set -eu

if [ "$(grep KAFFEINE_MAJOR_VERSION CMakeLists.txt)" == "" ]; then
  echo "Entering into the Kaffeine dir"
  cd kaffeine
fi

# not for production use

sourcedir=$(pwd)

cd ..

rm -rf kaffeine_build
git new-workdir $sourcedir kaffeine_build
cd kaffeine_build

rm -fr $sourcedir/install
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$sourcedir/install -DSTRICT_BUILD=1 .
make
make install
