#!/bin/bash

IMAGE="maurochehab/kaffeine"
WANTED_DEVS="/dev/dri /dev/dvb /dev/cdrom /dev/sr*"

if [ "$1" != "" ]; then
	IMAGE=$1
	shift
fi

xhost +SI:localuser:$(id -un)

DEVS=""
for i in $WANTED_DEVS; do
	if [ -d "$i" ]; then
		DEVS="$DEVS $i"
	else
		DEVS="$DEVS $(ls $i 2>/dev/null)"
	fi
done

GROUP_ADD=$(for i in $(id -G); do echo -n "--group-add=$i "; done)
docker run -it --env="DISPLAY" --env="QT_X11_NO_MITSHM=1" \
   --volume="/tmp/.X11-unix:/tmp/.X11-unix:rw" \
   $GROUP_ADD --user $(id -u $USER):$(id -g $USER) \
   --volume="$HOME:/home/kaffeine:rw" \
   $(for i in $DEVS; do echo -n "--device=$i:$i:rwm "; done) \
   $IMAGE $@
