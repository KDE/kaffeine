#!/bin/sh
set -eu

if [ "$(grep KAFFEINE_MAJOR_VERSION CMakeLists.txt)" == "" ]; then
  echo "Entering into the Kaffeine dir"
  cd kaffeine
fi

rm -fr po
mkdir po
cd po
cat >CMakeLists.txt << EOF
find_package(Gettext REQUIRED)

IF(NOT GETTEXT_MSGFMT_EXECUTABLE)
   MESSAGE(FATAL_ERROR "Please install the msgfmt binary")
endif(NOT GETTEXT_MSGFMT_EXECUTABLE)

if(NOT GETTEXT_MSGMERGE_EXECUTABLE)
   MESSAGE(FATAL_ERROR "Please install the msgmerge binary")
endif(NOT GETTEXT_MSGMERGE_EXECUTABLE)

EOF

svn cat svn://anonsvn.kde.org/home/kde/trunk/l10n-kf5/subdirs >subdirs
SUBDIRS=$(cat subdirs | grep -vx "x-test")
rm subdirs

for SUBDIR in $SUBDIRS ; do
	if [ "$(svn ls svn://anonsvn.kde.org/home/kde/trunk/l10n-kf5/$SUBDIR/messages/extragear-multimedia/kaffeine.po 2>/dev/null|grep kaffeine.po)" != "" ]; then
		mkdir $SUBDIR
		svn cat svn://anonsvn.kde.org/home/kde/trunk/l10n-kf5/$SUBDIR/messages/extragear-multimedia/kaffeine.po > $SUBDIR/kaffeine.po || true
		if [ -e "$SUBDIR/kaffeine.po" ]; then
			echo "Downloaded $SUBDIR/kaffeine.po"
			echo "add_subdirectory($SUBDIR)" >>CMakeLists.txt
			echo "GETTEXT_PROCESS_PO_FILES($SUBDIR ALL INSTALL_DESTINATION \${LOCALE_INSTALL_DIR} kaffeine.po)" >$SUBDIR/CMakeLists.txt
		else
			echo "Failed to download $SUBDIR/kaffeine.po"
			rmdir $SUBDIR
		fi
	fi
done
