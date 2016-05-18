project(kaffeine)

cmake_minimum_required(VERSION 3.4)
set(QT_MIN_VERSION "4.6.0")

find_package(Qt5Core)
if( Qt5Core_DIR )
    set(KF5_MIN_VERSION "5.1.0")
    message(STATUS "Found Qt5! Be aware that Qt5-support is still experimental and not officially supported!")

    find_package(ECM 1.0.0 REQUIRED NO_MODULE)

    set(CMAKE_MODULE_PATH
	${ECM_MODULE_PATH}
	${ECM_KDE_MODULE_DIR}
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules"
    )

    include(KDEInstallDirs)
    include(KDECMakeSettings)
    include(KDECompilerSettings)
    include(FeatureSummary)
    include(ECMInstallIcons)
    include(ECMOptionalAddSubdirectory)
    include(CheckIncludeFiles)

   # Instruct CMake to run moc automatically when needed.
   set(CMAKE_AUTOMOC ON)

    # Find Qt modules
    find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
	Core    # QCommandLineParser, QStringLiteral
	Widgets # QApplication
	Network
	Sql
	X11Extras
    )

    # Find KDE modules
    find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
	CoreAddons      # KAboutData
	I18n            # KLocalizedString
	WidgetsAddons   # KMessageBox
	XmlGui		# KActonCollection
	KIO		# KFileWidget
	Solid
	Notifications
    )

    include(KDEInstallDirs)
    include(KDECompilerSettings)
    include(KDECMakeSettings)
    include(FeatureSummary)
else(Qt5Core_DIR)
    set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
    find_package(KDE4 REQUIRED)
    if(STRICT_BUILD)
	add_definitions(-Werror -Weverything -Wno-packed -Wno-padded -Wno-sign-conversion -Wno-unreachable-code
			-Wno-weak-vtables -ferror-limit=5
			-isystem /usr/include/KDE
			-isystem /usr/include/qt4/QtCore
			-isystem /usr/include/qt4/QtGui
			-isystem /usr/include/qt4/QtNetwork
			-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_CAST_TO_ASCII
			-DQT_NO_URL_CAST_FROM_STRING -DQT_STRICT_ITERATORS)
    endif(STRICT_BUILD)
endif(Qt5Core_DIR)

find_package(X11 REQUIRED)
find_package(VLC 1.2 REQUIRED)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR} ${VLC_INCLUDE_DIRS} ${LIBDVBV5_INCLUDE_DIR}
                    ${X11_Xscreensaver_INCLUDE_PATH})

check_include_files(${CMAKE_CURRENT_SOURCE_DIR}/include/frontend.h HAVE_DVB)

if(NOT HAVE_DVB)
  MESSAGE(STATUS "Disabled features:\nDVB  Digital television support for Linux\n")
  set(HAVE_DVB 0)
endif(NOT HAVE_DVB)

if(HAVE_DVB)
  find_package(LibV4L2)
  if(NOT LIBDVBV5_FOUND)
    set(HAVE_LIBDVBV5 0)
    MESSAGE(STATUS "Disabled features:\nDVB  Digital television support for Linux, as libdvbv5 was not found.\n")
  endif(NOT LIBDVBV5_FOUND)
endif(HAVE_DVB)

add_subdirectory(deviceactions)
add_subdirectory(dtvdaemon)
add_subdirectory(icons)
add_subdirectory(profiles)
add_subdirectory(src)
ecm_optional_add_subdirectory(po)

if(BUILD_TOOLS)
  add_subdirectory(tools)
endif(BUILD_TOOLS)
