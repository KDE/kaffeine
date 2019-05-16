# Copyright(c) Mauro Carvalho Chehab <mchehab+samsung@kernel.org>
#
# Released under the terms of GPL 2.0.


# Please notice that this container uses X11. So, it has to run with:
#
# docker run -it --env="DISPLAY" --env="QT_X11_NO_MITSHM=1" --volume="/tmp/.X11-unix:/tmp/.X11-unix:rw" --volume="$HOME:/home/kaffeine:rw"  $(for i in $(ls /dev/dvb/adapter*/*); do echo -n "--device=$i:$i:rwm "; done) maurochehab/kaffeine
#

FROM ubuntu:19.04

MAINTAINER Mauro Carvalho Chehab <mchehab+samsung@kernel.org>

# STEP 1: Install build and runtime dependencies

RUN echo "deb http://archive.ubuntu.com/ubuntu disco main restricted universe multiverse" >/etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y --no-install-recommends build-essential cmake \
    pkg-kde-tools pkg-config extra-cmake-modules qtbase5-dev libqt5x11extras5-dev \
    libkf5coreaddons-dev libkf5dbusaddons-dev libkf5i18n-dev libkf5kio-dev debhelper \
    libkf5solid-dev libkf5widgetsaddons-dev libkf5windowsystem-dev libkf5xmlgui-dev \
    libkf5doctools-dev git libx11-dev libxss-dev libudev-dev libvlc-dev libqt5dbus5 \
    dh-autoreconf autotools-dev autoconf-archive libtool pkg-config libqt5sql5-sqlite \
    appstream dbus-x11 wget openssl ca-certificates && \
    apt-get install -y --no-install-recommends vlc-data vlc-plugin-base vlc-plugin-qt \
    vlc-plugin-video-output && \
    rm -rf /var/lib/apt/lists/*

# STEP 2: Build v4l-utils and Kaffeine from their sources

RUN cd ~ && git clone git://linuxtv.org/v4l-utils.git && \
    cd ~/v4l-utils && ./bootstrap.sh && ./configure \
    --disable-bpf --disable-qvidcap --disable-qv4l2 --disable-v4l-utils --disable-dyn-libv4l \
    && make && make install

# STEP 3: Build Kaffeine from their sources with newest scanfile.dvb

RUN cd ~ && git clone git://anongit.kde.org/kaffeine.git && \
    wget https://linuxtv.org/downloads/dtv-scan-tables/kaffeine/scantable.dvb -O ~/kaffeine/src/scanfile.dvb && \
    cd ~/kaffeine && cmake . && make VERBOSE=1 && make install

# STEP 4: purge development dependencies

RUN apt-get purge -y qtbase5-dev libqt5x11extras5-dev \
    libkf5coreaddons-dev libkf5dbusaddons-dev libkf5i18n-dev libkf5kio-dev debhelper \
    libkf5solid-dev libkf5widgetsaddons-dev libkf5windowsystem-dev libkf5xmlgui-dev \
    libkf5doctools-dev git libx11-dev libxss-dev libudev-dev libvlc-dev \
    dh-autoreconf autotools-dev autoconf-archive libtool pkg-config wget && \
    apt-get clean -y && rm -rf ~/kaffeine ~/v4l-utils

# STEP 5: create an user to run it

RUN useradd -m -r -u 1000 -g users -Gaudio,video,irc kaffeine
USER kaffeine

# STEP 6: command to run the container

#CMD ls -la /dev/dvb/adapter0
CMD export $(dbus-launch); /usr/local/bin/kaffeine
