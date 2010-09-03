#!/bin/sh
set -eu

cd kaffeine
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

wget http://websvn.kde.org/*checkout*/trunk/l10n-kde4/subdirs
SUBDIRS=$(cat subdirs | grep -vx "x-test")
rm subdirs

for SUBDIR in $SUBDIRS ; do
	mkdir $SUBDIR
	cd $SUBDIR
	wget http://websvn.kde.org/*checkout*/trunk/l10n-kde4/$SUBDIR/messages/extragear-multimedia/kaffeine.po || true
	cd ..

	if test -e $SUBDIR/kaffeine.po ; then
		echo "add_subdirectory($SUBDIR)" >>CMakeLists.txt
		echo "GETTEXT_PROCESS_PO_FILES($SUBDIR ALL INSTALL_DESTINATION \${LOCALE_INSTALL_DIR} kaffeine.po)" >$SUBDIR/CMakeLists.txt
	else
		rmdir $SUBDIR
	fi
done
