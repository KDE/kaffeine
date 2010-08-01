#!/bin/sh
set -eu

cd kaffeine/include
rm -fr *
wget http://linuxtv.org/hg/v4l-dvb/raw-file/tip/linux/include/linux/dvb/dmx.h
wget http://linuxtv.org/hg/v4l-dvb/raw-file/tip/linux/include/linux/dvb/frontend.h
mv dmx.h dmx.h.1
mv frontend.h frontend.h.1
unifdef -k -U__KERNEL__ dmx.h.1 >dmx.h || true
unifdef -k -U__KERNEL__ frontend.h.1 >frontend.h || true
sed 'N;s/,\n\}/\n\}/;P;D;' -i dmx.h
sed 'N;s/,\n\}/\n\}/;P;D;' -i frontend.h
rm *.1
