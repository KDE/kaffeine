#!/bin/sh
set -eu

if [ "$(grep KAFFEINE_MAJOR_VERSION CMakeLists.txt)" == "" ]; then
  echo "Entering into the Kaffeine dir"
  cd kaffeine
fi

BASE_URL="https://git.linuxtv.org/media_tree.git"

cd include
rm dmx.h frontend.h

wget -nv $BASE_URL/plain/include/uapi/linux/dvb/dmx.h
wget -nv $BASE_URL/plain/include/uapi/linux/dvb/frontend.h
unifdef -k -U__KERNEL__ -o dmx.h dmx.h || true
unifdef -k -U__KERNEL__ -o frontend.h frontend.h || true
sed 'N;s/,\n\}/\n\}/;P;D;' -i dmx.h
sed 'N;s/,\n\}/\n\}/;P;D;' -i frontend.h
sed 's/_UAPI_/_/g' -i dmx.h
