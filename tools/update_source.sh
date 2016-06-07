#!/bin/sh
set -eu

# not for production use

if [ "$(grep KAFFEINE_MAJOR_VERSION CMakeLists.txt)" == "" ]; then
  echo "Entering into the Kaffeine dir"
  cd kaffeine
fi

cmake -DBUILD_TOOLS=1 .
make updatesource

tools/updatesource
