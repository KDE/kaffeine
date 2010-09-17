#!/bin/sh
set -eu

cd kaffeine/icons
rm -f hi*

if [ `find /usr/share/icons/oxygen | grep -i kaffeine | wc --lines` != 6 ] ; then
	echo "recheck number of icons"
	exit 1
fi

for SIZE in 16 22 32 48 64 128 ; do
	wget http://websvn.kde.org/*checkout*/trunk/kdesupport/oxygen-icons/$SIZE'x'$SIZE/apps/kaffeine.png -O hi$SIZE-apps-kaffeine.png
done

wget http://websvn.kde.org/*checkout*/trunk/kdesupport/oxygen-icons/scalable/apps/kaffeine.svgz -O hisc-apps-kaffeine.svgz
