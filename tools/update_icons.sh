#!/bin/sh
set -eu

if [ "$(grep KAFFEINE_MAJOR_VERSION CMakeLists.txt)" == "" ]; then
  echo "Entering into the Kaffeine dir"
  cd kaffeine
fi

rm -f icons/hi*

if [ `find /usr/share/icons/oxygen | grep -i kaffeine | wc --lines` != 6 ] ; then
	echo "recheck number of icons"
	exit 1
fi

for SIZE in 16 22 32 48 64 128 ; do
	wget -nv "https://quickgit.kde.org/?p=oxygen-icons5.git&a=blob&f=${SIZE}x$SIZE%2Fapps%2Fkaffeine.png&o=plain" -O icons/hi$SIZE-apps-kaffeine.png
done

wget -nv "https://quickgit.kde.org/?p=oxygen-icons5.git&a=blob&f=scalable%2Fapps%2Fkaffeine.svgz&o=plain" -O icons/hisc-apps-kaffeine.svgz
