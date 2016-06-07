#!/bin/sh

if [ "$1" == "" ]; then
  echo "$0 <dtv-scan-tables tree directory>"
  exit 1
fi
SCAN_DIR="$1"

set -eu

if [ "$(grep KAFFEINE_MAJOR_VERSION CMakeLists.txt)" == "" ]; then
  echo "Entering into the Kaffeine dir"
  cd kaffeine
fi

cmake . -DBUILD_TOOLS=1
make convertscanfiles

(cd $SCAN_DIR; git remote update; git rebase origin/master)

tools/convertscanfiles /devel/v4l/dtv-scan-tables ../kaffeine/src/scanfile.dvb
