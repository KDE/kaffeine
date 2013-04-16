#!/bin/sh
set -eu

cd include
rm dmx.h frontend.h
wget https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/plain/include/uapi/linux/dvb/dmx.h
wget https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/plain/include/uapi/linux/dvb/frontend.h
unifdef -k -U__KERNEL__ -o dmx.h dmx.h || true
unifdef -k -U__KERNEL__ -o frontend.h frontend.h || true
sed 'N;s/,\n\}/\n\}/;P;D;' -i dmx.h
sed 'N;s/,\n\}/\n\}/;P;D;' -i frontend.h
sed 's/_UAPI_/_/g' -i dmx.h
