#!/bin/bash

# This example map file relies on this grabber
# tv_grab_eu_dotmedia >eu_dotmedia.xmltv

# In order to get DVB Channel list, use:
# echo 'select name from Channels;' | sqlite3 ~/.local/share/kaffeine/sqlite.db

# Just a random senseless map - Modify it for your own needs,
# adding just the channels that didn't map by default
map[0]="action.sky.de;E! HD"
map[1]="13thstreet.de;RIT"
map[2]="1bar.dazn.de;TNT HD"

#
# Don't touch on anything below
#

if [ "$2" == "" ]; then
	echo "Usage: $0 <origin_file.xmltv> <dest_file.xmltv>"
	exit 1
fi

set -e

# Original file
orig="$1"

# Parsed file, to be used in Kaffeine
dest="$2"

tmpfile1="tmp1_$$.xmltv"
tmpfile2="tmp2_$$.xmltv"

trap "rm -f $tmpfile1 $tmpfile2 2>/dev/null" EXIT
cp $orig $tmpfile2

IFS=$','
for i in ${map[@]}; do
	array=(${i/;/,})
	channel=${array[0]}
	name=${array[1]}

	echo "$channel -> $name"

	mv $tmpfile2 $tmpfile1
	xmlstarlet ed -s "tv/channel[@id='$channel']" -t elem -n display-name \
	-v "$name" $tmpfile1 > $tmpfile2
done
mv $tmpfile2 $dest
