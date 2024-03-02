cmake_minimum_required(VERSION 3.16)

project(kaffeine)

set(KAFFEINE_MAJOR_VERSION 2)
set(KAFFEINE_MINOR_VERSION 0)
set(KAFFEINE_PATCH_VERSION 19)

# comment/uuncomment below if either the version is a production or a development one
#set(KAFFEINE_EXTRA_VERSION -git)

set(KAFFEINE_VERSION
  ${KAFFEINE_MAJOR_VERSION}.${KAFFEINE_MINOR_VERSION}.${KAFFEINE_PATCH_VERSION}${KAFFEINE_EXTRA_VERSION})

option(BUILD_TOOLS "Build the helper tools" OFF)

set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "6.0.0")

find_package(Qt6Core)
find_package(ECM 1.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH
	${ECM_MODULE_PATH}
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
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
	Core    # QCommandLineParser, QStringLiteral
	Widgets # QApplication
	Network
	Sql
)

# Find KDE modules
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
	CoreAddons      # KAboutData
	I18n            # KLocalizedString
	WidgetsAddons   # KMessageBox
	WindowSystem	# KStartupInfo
	XmlGui		# KActonCollection
	KIO		# KFileWidget
	Solid
	DBusAddons
)
find_package(KF6DocTools ${KF6_MIN_VERSION} QUIET)

find_package(VLC 1.2 REQUIRED)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  # FreeBSD has a fully-working v4l, but needs help finding it
  unset(_dvb_headers)
  find_package(Libdvbv5)
  if (Libdvbv5_FOUND)
    find_path(_dvb_headers
      NAMES dmx.h
      PATH_SUFFIXES dvb linux/dvb
    )
    if(_dvb_headers)
      include_directories(${_dvb_headers})
    endif()
  endif()
  if(NOT _dvb_headers)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
  endif()
else()
  # Linux (and others) have a weird copy of v4l headers to use
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
include_directories(${CMAKE_BINARY_DIR} ${VLC_INCLUDE_DIRS})

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
add_subdirectory(icons)
add_subdirectory(src)

ki18n_install(po)

if(KF6DocTools_VERSION)
  add_subdirectory(doc)
  kdoctools_install(po)
endif(KF6DocTools_VERSION)

if(BUILD_TOOLS)
  add_subdirectory(tools)
endif(BUILD_TOOLS)
