#! /bin/sh
$EXTRACTRC `find . -name \*.ui` >> rc.cpp
$XGETTEXT *.cpp */*.cpp -o $podir/kaffeine4.pot
rm -f rc.cpp
