#!/bin/sh
set -eu

if [ "$(grep KAFFEINE_MAJOR_VERSION CMakeLists.txt)" == "" ]; then
  echo "Entering into the Kaffeine dir"
  cd kaffeine
fi

cmake -DBUILD_TOOLS=1 .
make updatedvbsi
cd tools
./updatedvbsi
