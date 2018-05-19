Installing Kaffeine
===================

Prerequisites
-------------

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

### Debian Packaging

If you want to create a Debian package from Kaffeine's sources,
you need to install the needed tools with:

    apt-get install fakeroot dpkg-dev pkg-kde-tools dephelper

And run the following commands:

    git clone git://anonscm.debian.org/pkg-kde/kde-extras/kaffeine.git deb-build
    cd deb-build
    rsync -ua --exclude '.git*' --exclude deb-build .. .
    rm CMakeCache.txt
    cat Changelog |grep Version|head -1|perl -ne 'if (m/Version\s*(\S+)/) { print "kaffeine ($1-1) unstable; urgency=medium\n\n  * New upstream release.\n" }' >debian/changelog
    echo " -- root <root@localhost>  $(date -R)" >>debian/changelog
    fakeroot debian/rules binary

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

Installing translations
-----------------------

This step is optional, and should be done only if you want to
use Kaffeine on non-English setups.

The Kaffeine tarballs should already have the translations on it,
but, if you're installing from the git tree, you'll need to run a
script to get them:

    (cd .. && kaffeine/tools/update_l10n.sh)

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
