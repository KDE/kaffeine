# Please notice that this container uses X11. So, it has to run with:
# docker run -it --env="DISPLAY" --env="QT_X11_NO_MITSHM=1" --volume="/tmp/.X11-unix:/tmp/.X11-unix:rw" kaffeine
#

FROM ubuntu:19.04

MAINTAINER Mauro Carvalho Chehab <mchehab+samsung@kernel.org>

# STEP 1: Install build dependencies

RUN echo "deb http://archive.ubuntu.com/ubuntu disco main restricted universe multiverse" >/etc/apt/sources.list
RUN apt-get update
RUN apt-get install -y  build-essential cmake \
    pkg-kde-tools pkg-config extra-cmake-modules qtbase5-dev libqt5x11extras5-dev \
    libkf5coreaddons-dev libkf5dbusaddons-dev libkf5i18n-dev libkf5kio-dev debhelper \
    libkf5solid-dev libkf5widgetsaddons-dev libkf5windowsystem-dev libkf5xmlgui-dev \
    libkf5doctools-dev git libx11-dev libxss-dev libudev-dev libvlc-dev libdvbv5-dev \
    libqt5dbus5 appstream dbus-x11

# STEP 2: Build Kaffeine from its sources

RUN cd ~ && git clone git://anongit.kde.org/kaffeine.git
RUN cd ~/kaffeine && cmake . && make VERBOSE=1 && make install && cd ..

# STEP 3: Add runtime dependencies

RUN apt-get install -y --no-install-recommends vlc-data vlc-plugin-base vlc-plugin-qt \
    vlc-plugin-video-output

# STEP 4: purge development dependencies

RUN apt-get purge -y qtbase5-dev libqt5x11extras5-dev \
    libkf5coreaddons-dev libkf5dbusaddons-dev libkf5i18n-dev libkf5kio-dev debhelper \
    libkf5solid-dev libkf5widgetsaddons-dev libkf5windowsystem-dev libkf5xmlgui-dev \
    libkf5doctools-dev git libx11-dev libxss-dev libudev-dev libvlc-dev libdvbv5-dev
RUN apt-get clean -y && rm -rf /var/lib/apt/lists/* ~/kaffeine

# STEP 5: create an user to run it

RUN useradd -m -r -u 1000 -g users -Gaudio,video,irc kaffeine
USER kaffeine

# STEP 6: command to run the container

#CMD ls -la /dev/dvb/adapter0
CMD export $(dbus-launch); /usr/local/bin/kaffeine
