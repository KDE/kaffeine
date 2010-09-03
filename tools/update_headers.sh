#!/bin/sh
set -eu

cd kaffeine/include
rm -fr *
wget "http://git.linuxtv.org/linux-2.6.git?a=blob_plain;f=include/linux/dvb/dmx.h;hb=HEAD" -O dmx.h.1
wget "http://git.linuxtv.org/linux-2.6.git?a=blob_plain;f=include/linux/dvb/frontend.h;hb=HEAD" -O frontend.h.1
unifdef -k -U__KERNEL__ dmx.h.1 >dmx.h || true
unifdef -k -U__KERNEL__ frontend.h.1 >frontend.h || true
sed 'N;s/,\n\}/\n\}/;P;D;' -i dmx.h
sed 'N;s/,\n\}/\n\}/;P;D;' -i frontend.h
rm *.1
