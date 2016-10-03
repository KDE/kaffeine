#!/bin/sh
set -eu

if [ "$(grep KAFFEINE_MAJOR_VERSION CMakeLists.txt)" == "" ]; then
  echo "Entering into the Kaffeine dir"
  cd kaffeine
fi

cmake . -DBUILD_TOOLS=1

make -C tools updatemimetypes

echo
echo "$ tools/updatemimetypes src/org.kde.kaffeine.desktop src/mediawidget.cpp"
tools/updatemimetypes src/org.kde.kaffeine.desktop src/mediawidget.cpp
