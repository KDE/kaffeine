#!/bin/sh
$XGETTEXT *.cpp */*.cpp -o $podir/kaffeine.pot
sed "s/, c-format/, no-c-format/" -i $podir/kaffeine.pot
