project(kaffeine)

set(KAFFEINE_MAJOR_VERSION 2)
set(KAFFEINE_MINOR_VERSION 0)
set(KAFFEINE_PATCH_VERSION 9)

# comment/uuncomment below if either the version is a production or a development one
#set(KAFFEINE_EXTRA_VERSION -git)

set(KAFFEINE_VERSION
  ${KAFFEINE_MAJOR_VERSION}.${KAFFEINE_MINOR_VERSION}.${KAFFEINE_PATCH_VERSION}${KAFFEINE_EXTRA_VERSION})

option(BUILD_TOOLS "Build the helper tools" OFF)

# Minimal versions where build is known to work
cmake_minimum_required(VERSION 2.8.12)
set(QT_MIN_VERSION "5.4.0")
set(KF5_MIN_VERSION "5.11.0")

# shut up cmake warnings about newer policies
if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif(POLICY CMP0063)

## workaround for ki18n 5.34.0++ changes (see BUG 379834)
if(POLICY CMP0002)
  cmake_policy(SET CMP0002 OLD)
endif(CMP0002)

find_package(Qt5Core)
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
	DBusAddons
)
find_package(KF5DocTools ${KF5_MIN_VERSION} QUIET)

find_package(X11 REQUIRED)
find_package(VLC 1.2 REQUIRED)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR} ${VLC_INCLUDE_DIRS}
                    ${X11_Xscreensaver_INCLUDE_PATH})

check_include_files(${CMAKE_CURRENT_SOURCE_DIR}/include/frontend.h HAVE_DVB)

if(NOT HAVE_DVB)
  MESSAGE(STATUS "Disabled features:\nDVB  Digital television support for Linux\n")
  set(HAVE_DVB 0)
else()
  set(HAVE_DVB 1)
endif(NOT HAVE_DVB)

if(HAVE_DVB)
  find_package(Libdvbv5)
  if(NOT Libdvbv5_FOUND)
    set(HAVE_LIBDVBV5 0)
    set(HAVE_DVB 0)
    message(STATUS "Disabled features:\nDVB  Digital television support for Linux, as libdvbv5 was not found.\n")
  endif(NOT Libdvbv5_FOUND)
endif(HAVE_DVB)

add_subdirectory(deviceactions)
add_subdirectory(dtvdaemon)
add_subdirectory(icons)
add_subdirectory(profiles)
add_subdirectory(src)
if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/po")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/po/CMakeLists.txt" )
    add_subdirectory(po)
  else()
    ki18n_install(po)
  endif()
endif()

if(KF5DocTools_VERSION)
  add_subdirectory(doc)
endif(KF5DocTools_VERSION)

if(BUILD_TOOLS)
  add_subdirectory(tools)
endif(BUILD_TOOLS)
