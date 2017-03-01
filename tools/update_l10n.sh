#!/bin/sh
set -eu

SVN_REPO="svn://anonsvn.kde.org/home/kde/"
# SVN_REPO="svn://svn@svn.kde.org/home/kde/"

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

svn cat $SVN_REPO/trunk/l10n-kf5/subdirs >subdirs

trap "rm /tmp/teamnames" 1 2 3 6 15 # HUP INT QUIT ABRT TERM
svn cat $SVN_REPO/trunk/l10n-kf5/teamnames >/tmp/teamnames

SUBDIRS=$(cat subdirs | grep -vx "x-test")
rm subdirs

## HACK: Download subdirs currently with translated docs/man pages
#SUBDIRS="pt_BR ca de es it nl pt sv uk"

unset LANG_NAME
for SUBDIR in $SUBDIRS ; do
	LANG_NAME=$(grep "$SUBDIR=" /tmp/teamnames|cut -d'=' -f2)
	LANG_NAME=${LANG_NAME/ /-}

	DOCS=""

	# GUI messages
	if [ "$(svn ls $SVN_REPO/trunk/l10n-kf5/$SUBDIR/messages/extragear-multimedia/kaffeine.po 2>/dev/null|grep kaffeine.po)" != "" ]; then
		mkdir $SUBDIR
		svn cat $SVN_REPO/trunk/l10n-kf5/$SUBDIR/messages/extragear-multimedia/kaffeine.po > $SUBDIR/kaffeine.po || true
		if [ -s "$SUBDIR/kaffeine.po" ]; then
			DOCS="kaffeine.po $DOCS"
			echo "Downloaded $LANG_NAME translation: $SUBDIR/kaffeine.po"
		else
			echo "Failed to download $SUBDIR/kaffeine.po"
		fi
	fi

	# Docs
	FILES=$(svn ls $SVN_REPO/trunk/l10n-kf5/$SUBDIR/docmessages/extragear-multimedia 2>/dev/null|grep kaffeine) || true
	if [ "$FILES" != "" ]; then
		mkdir -p $SUBDIR/docs
		for i in $FILES; do
			j=$SUBDIR/docs/$i
			if [ "$i" == "kaffeine.po" ]; then
				j=$SUBDIR/docs/doc_$i
			else
				j=$SUBDIR/docs/$i
			fi
			svn cat $SVN_REPO/trunk/l10n-kf5/$SUBDIR/docmessages/extragear-multimedia/$i > $j || true
			if [ -s "$j" ]; then
				DOCS="$DOCS docs/$i"
				echo "Downloaded $LANG_NAME translation: $j"
			else
				echo "Failed to download $j"
			fi
		done
	fi

	if [ "$DOCS" == "" ]; then
		rm -rf $SUBDIR/
	else
		for i in $DOCS; do
			if [ "$(echo $i |grep docs/kaffeine.po)" != "" ]; then
				echo "add_custom_target(html-$SUBDIR ALL DEPENDS $SUBDIR/index.docbook)" >>CMakeLists.txt
				echo "add_custom_command(OUTPUT $SUBDIR/index.docbook COMMAND ../tools/po2html ../doc/index.docbook $SUBDIR/docs/doc_kaffeine.po $LANG_NAME $SUBDIR/index.docbook DEPENDS ../doc/index.docbook $SUBDIR/docs/doc_kaffeine.po COMMENT building index.docbook)" >>CMakeLists.txt
			elif [ "$(echo $i |grep docs/kaffeine_man-kaffeine.1.po)" != "" ]; then
				echo "add_custom_target(man-$SUBDIR ALL DEPENDS $SUBDIR/man-kaffeine.1.docbook)" >>CMakeLists.txt
				echo "add_custom_command(OUTPUT $SUBDIR/man-kaffeine.1.docbook COMMAND po2xml ../doc/man-kaffeine.1.docbook $SUBDIR/docs/kaffeine_man-kaffeine.1.po > $SUBDIR/man-kaffeine.1.docbook DEPENDS ../doc/man-kaffeine.1.docbook $SUBDIR/docs/kaffeine_man-kaffeine.1.po COMMENT building manpage VERBATIM)" >>CMakeLists.txt
				echo "kdoctools_create_manpage($SUBDIR/man-kaffeine.1.docbook 1 INSTALL_DESTINATION \${MAN_INSTALL_DIR})" >>CMakeLists.txt
			else
				echo "add_subdirectory($SUBDIR)" >>CMakeLists.txt
				echo "GETTEXT_PROCESS_PO_FILES($SUBDIR ALL INSTALL_DESTINATION \${LOCALE_INSTALL_DIR}/$SUBDIR PO_FILES $i)" >>$SUBDIR/CMakeLists.txt
			fi
		done
		echo >>CMakeLists.txt
	fi
done

rm /tmp/teamnames
