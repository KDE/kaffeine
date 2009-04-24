#! /bin/sh
$EXTRACTRC `find . -name \*.ui` >> rc.cpp
$XGETTEXT *.cpp */*.cpp -o $podir/kaffeine.pot
rm -f rc.cpp
