Installing Kaffeine
===================

Installing Prerequisites
------------------------

The following tools are needed to build Kaffeine:

* GNU c++
* GNU make
* cmake >= 2.8
* cmake ECM (extra-cmake-modules)

The following development headers are needed (recommended versions):

* Qt >= 5.4
* KF5 >= 5.11
* libX11
* libXss
* libqt-sql-sqlite
* libvlc
* libdvbv5

If you also want language translations you also need:

* gettext

If you also want the Kaffeine Handbook, you also need:

* KF5DocTools

For runtime translations of the ISO 639 language codes, you also need:

* iso-codes

Debian and Ubuntu
-----------------

The needed packages for Debian/Ubuntu should be installed with:

    apt-get install kdelibs5-dev libvlc-dev libxss-dev vlc \
		   libkf5coreaddons-dev libkf5i18n-dev libqt5x11extras5-dev \
		   libkf5windowsystem-dev \
		   libkf5solid-dev libkf5widgetsaddons-dev kio-dev \
		   qt5-default libdvbv5-dev \
		   cmake extra-cmake-modules make g++ gettext

And, to build the optional Kaffeine Handbook documentation:

    apt-get install kdoctools-dev

PS.: The above was tested with Debian SID and Ubuntu Xenial (16.04).
     Other versions may have different requirements.

Fedora
------

On Fedora, you need a repository that provides VLC.

For stable fedora releases, you could use the rpmfusion repository.
For Fedora 22 and later, it can be installed with:

	sudo dnf install https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

See <http://rpmfusion.org/Configuration/> for more details.

For Fedora rawhide and beta releases, you could use, instead the
Unitedrpms repository: <https://unitedrpms.github.io/>. Please read
at <https://github.com/UnitedRPMs/unitedrpms.github.io/blob/master/README.md>
for instructions about how to set up.

Once the repository with VLC is set, install the needed packages
with:

    dnf install  kf5-kcoreaddons-devel libXScrnSaver-devel \
		 qt5-qtx11extras-devel libdvbv5 \
		 kf5-kwindowsystem-devel kf5-solid-devel kf5-kio-devel \
		 kf5-kdbusaddons-devel kf5-ki18n-devel vlc-devel gettext-devel \
		 cmake extra-cmake-modules make gcc-c++ gettext

And, to build the Kaffeine Handbook documentation:

    dnf install kf5-kdoctools-devel

openSUSE
--------

If you run openSUSE Tumbleweed, you can find an up-to-date package with the
latest state of git in the KDE:Unstable:Extra repository.

    zypper ar obs://KDE:Unstable:Extra KDE_Unstable_Extra # add repository
    zypper in -r KDE_Unstable_Extra kaffeine

If you are using openSUSE Leap or openSUSE 13.2, you will need to compile from
sources instead.  You need to be using at least OpenSUSE version 13.2, in order to have
KF5 and Qt 5.5 at their repositories.

The first step is to install the needed dependencies:

    zypper install extra-cmake-modules vlc-devel make gcc gcc-g++ cmake \
	   libqt5-qtbase-devel libqt5-qtx11extras-devel \
	   kdbusaddons-devel solid-devel kio-devel ki18n-devel

If you're running OpenSUSE version 13.2, you'll need to compile the
libdvbv5 by hand, as it is not provided there. OpenSUSE Leap
(version 42.1) seem to have it already packaged as libdvbv5-devel.

Before compiling libdvbv5, some packages are needed:

    zypper install autoconf automake libjpeg-devel

Compiling libdvbv5 (as normal user):

    wget https://linuxtv.org/downloads/v4l-utils/v4l-utils-1.10.0.tar.bz2
    tar xvf v4l-utils-1.10.0.tar.bz2
    cd v4l-utils
    ./bootstrap.sh && ./configure && make

Installing the library (as root):

    make install

And, to build the optional Kaffeine Handbook documentation:

    zypper install kdoctools-devel

PS.: The above was tested with openSUSE 13.2.
     Other versions may have different requirements.

Gentoo
------

Kaffeine is already packaged on Gentoo. Installing it is as
simple as:

    emerge kaffeine

Arch Linux
----------

Kaffeine is already packaged on Arch Linux. Installing it is as
simple as:

    pacman -S kaffeine

How to build Kaffeine
=====================

Create an empty build directory and do the following steps:

    $ cmake <path/to/kaffeine> <options>
    $ make

Where `path/to/kaffeine` is usually the current dir, e. g., the
following command is usually enough:

    $ cmake . && make

Useful `options` include:

* -DCMAKE_BUILD_TYPE=<type> (Debug or Release)
* -DCMAKE_INSTALL_PREFIX=<path> (installation prefix for Kaffeine, e.g. /usr)
* -DBUILD_TOOLS=1 (also compile some tools needed by developers)

You may also use `ccmake` if you want to see all Kaffeine's build
options, and set them using an interactive interface.

For further information look for generic KF5 / cmake instructions.

The install should be done as root user with:

    # make install

How to produce a Debian/Ubuntu package
======================================

If you want to create a Debian or Ubuntu package from Kaffeine's sources,
you need to install the needed tools with:

    apt-get install fakeroot dpkg-dev pkg-kde-tools debhelper

And run the following commands:

    rm -rf deb-build  # Just in case it were created before

    git clone https://salsa.debian.org/qt-kde-team/extras/kaffeine.git deb-build && \
    cd deb-build && \
    rsync -ua --exclude '.git*' --exclude deb-build .. . && \
    rm CMakeCache.txt && \
    cat Changelog |grep Version|head -1|perl -ne 'if (m/Version\s*(\S+)/) { print "kaffeine ($1-1) unstable; urgency=medium\n\n  * New upstream release.\n" }' >debian/changelog && \
    echo " -- root <root@localhost>  $(date -R)" >>debian/changelog && \
    fakeroot debian/rules binary && \
    cd ..

This will produce both binary and debug packages, like:

	kaffeine_*_amd64.deb  kaffeine-dbgsym_*_amd64.ddeb

Installing it is as simple as:

	sudo dpkg -i kaffeine_*_amd64.deb

Running Kaffeine from Docker
============================

It is now possible to run kaffeine from Docker without installing it
on your machine by using Docker. You need to have Docker already
installed and configured.

Installing Docker
-----------------

The prodedures for installing Docker vary from Distribution. It usually
involves using the distro package manager to install it from the
usual repositories:

	sudo dnf install docker                         # Fedora
	sudo apt install docker.io                      # Debian/Ubuntu
	sudo zypper install docker docker-compose       # SUSE
	sudo emerge app-emulation/docker                # Gentoo
	sudo pacman -S docker                           # Arch Linux

(some distros like Gentoo may require additional steps)

Then add the current user into a docker group, in order to allow
itr to run the docker command without sudo:

	sudo groupadd docker            # if the docker group doesn't exist yet
	sudo gpasswd -a $USER docker
	newgrp docker                   # or close the session and re-open

Finally, ask systemd to enable dockerd at boot time and to (re)start it:

	sudo systemctl enable docker
	sudo systemctl restart docker

In case of doubts, please seek at the Internet for a guide especific
for the distro you're using.

Running Kaffeine's docker image
-------------------------------

Once you have docker installed, you can download the Kaffeine's
Docker image from:

- <https://cloud.docker.com/u/maurochehab/repository/docker/maurochehab/kaffeine/general>

This can easily be done by running:

	docker pull maurochehab/kaffeine

Note: docker pull will also update the image to the newest one, in
case you have download it already.

The updated instructions about how to download and use the Docker
image it will be pointed there.

Please notice that Docker works by running an image created from the
Dockerfile packaged with Kaffeine. Such image is executed inside
a container, so it needs permission to open files, sockets and
devices from the host machine.

Getting the right mapping for volumes, groups and devices is tricky.
So, we have a script that should hopefully do what it is expected
(tested with Docker version 1.13):

	./kaffeine-docker.sh

There are some limitations when running Kaffeine via a docker
container:

1) The container runs with a non-root user (user ID is 1000). This
   is a requirement, as libVlc refuses to run as root. The user ID
   1000 is usually the default for the first user on most modern
   distros. If your user is different, the container won't get the
   right permissions to read files from your home dir and will fail.
   So, if you use a different user ID, you may need to re-generate
   the container locally.

2) In order to avoid legal discussions, the docker image hosted at
   docker.io doesn't contain the packages that are needed for DVD
   navigation and for decrypt DVD streams. If you need such features,
   you need to add the packages `libdvdread4` and `libdvdnav` to
   the Dockerfile and re-generate the container locally.

3) As the device map happens before starting the container,
   Kaffeine's capability of auto-detecting device hot-plug won't
   work.

Generating the Kaffeine container locally
-----------------------------------------

Building a Kaffeine's container should be as easy as running this command:

	docker build -t "mykaffeine" .

It should take a while for it to download the base image and the
needed packages, before starting build the container.

When using a local container, you need to tell the script to use the
new docker image you created. You do that by passing one parameeter to
the `kernel-docker.sh` script with the name of the image you created, e. g.:

	./kaffeine-docker.sh "mykaffeine"

Known video output issues
=========================

There are a few known issues related to video output that may require
some special setup for Kaffeine to work on certain environments.

Remote Access and Kaffeine
--------------------------

Accessing Kaffeine remotely via X11/ssh/vnc can be a problem, as Qt5 will
try, by default, to use hardware acceleration and DRI3.

There is a known bug, present on Fedora 23 to 25, and likely on other distros,
at mesa-libGL/dri-drivers that cause it to wait forever when it is started
from a X11 section. Such bug causes Kaffeine windows to not open:

* <https://bugzilla.redhat.com/show_bug.cgi?id=1174257>

A workaround is to start Kaffeine with:

    LIBGL_DRI3_DISABLE=1 kaffeine

Another solution is to use a vnc server.

Changing the libVLC output plugin
---------------------------------

By default, libVLC will try to use hardware acceleration on the machine with
Kaffeine, with obviously with won't work via remote access. It may also not
detect properly the best video output plugin for some hardware settings.

For such scenarios, you may try to change the arguments passed to libVLC via
the `Settings` -->  `Configure Kaffeine` -->  `libVLC`, changing the libVLC
arguments to:

    --no-video-title-show -V xcb_glx

or:

    --no-video-title-show -V xcb_xv

and re-start Kaffeine.

Setting VDPAU acceleration
--------------------------

By default, libVlc will try to use vdpau hardware acceleration in order to
decode the video stream at GPU. However, sometimes it may not get the right
acceleration module, trying to always use NVidia module, even when the
hardware is AMD or Intel. That happens, for example on Fedora 25 and 26, as
reported at:

* <https://bugzilla.redhat.com/show_bug.cgi?id=1305699>
* <https://bugs.kde.org/show_bug.cgi?id=376893>

For Radeon GPU, the vdpau driver can be forced with:

    export VDPAU_DRIVER=r600

The VA-GL driver can be used for Intel GPUs. It can be forced with:

    export VDPAU_DRIVER=va_gl

Note: you may need to install mesa-vdpau-drivers and/or libvdpau-va-gl
packages for vdpau to work.

Please notice that, depending on your hardware, for example, if your
GPU vdpau driver can't decode the compression standard used by the
broadcasters, it could be better to disable VDPAU backend.
That can be done by passing an invalid driver name, like:

    export VDPAU_DRIVER=none

Using xmltv for EPG data
========================

As described at Kaffeine's documentation, xmltv files are now supported.

In order to use it, you need to have a xmltv grabber. For example, you
may use the tv_grab_eu_dotmedia grabber, from xmltv project.

Kaffeine's internal logic will map the channels obtained by the grabber
into the channel names it has stored internally. The Kaffeine names
can be obtained with the following command:

    $ echo 'select name from Channels;' | sqlite3 ~/.local/share/kaffeine/sqlite.db

At the xmltv file format, the channel names can be obtained with:

    $ grep display-name some_file.xmltv

If the names don't match, you'll need to use an external script to do the
map. There's an example about how to do it at:

    tools/map_xmltv_channels.sh

Please notice that you need to have `xmlstarlet` installed for it to work.

Assuming that you modified the script for your needs and copied to your
`~/bin` directory, a typical usage of obtaining the xmltv tables would be to
have a script like this running on a shell console:

    $ while :; do tv_grab_eu_dotmedia > eu_dotmedia.xmltv; \
    ~/bin/map_xmltv_channels.sh eu_dotmedia.xmltv eu_dotmedia-new.xmltv; \
    sleep 3600; done

And configure Kaffeine to use the `eu_dotmedia-new.xmltv` file produced by
the script (or whatever other name you use), disabling EPG MPEG-TS table
reads.

Please also notice that as soon as Kaffeine detects a change on a file, it
will re-read. So, even if you don't need to do any map, you should
first generate the xmltv file and then move it to the right place, e. g:

    $ while :; do tv_grab_eu_dotmedia > eu_dotmedia.xmltv; \
    mv eu_dotmedia.xmltv eu_dotmedia-new.xmltv; \
    sleep 3600; done

For more details, please read Kaffeine's manual.

Homepage
========

<https://www.kde.org/applications/multimedia/kaffeine/>

Authors
=======

Maintainer since KF5/Qt5 port (version 2.x):

* Mauro Carvalho Chehab <mchehab+samsung@kernel.org>

Former maintainers:

* Lasse Lindqvist <lasse.k.lindqvist@gmail.com>
* Christoph Pfister
* Christophe Thommeret
* Jürgen Kofler

Thanks to various contributors, translators, testers ...
