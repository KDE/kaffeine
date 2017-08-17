if(Libdvbv5_FOUND)
  include_directories(${Libdvbv5_INCLUDE_DIRS})
endif(Libdvbv5_FOUND)

qt5_add_resources(RESOURCE_ADDED kaffeine.qrc)

set(kaffeine_SRCS
    kaffeine.qrc
    backend-vlc/vlcmediawidget.cpp
    playlist/playlistmodel.cpp
    playlist/playlisttab.cpp
    abstractmediawidget.cpp
    configuration.cpp
    configurationdialog.cpp
    datetimeedit.cpp
    dbusobjects.cpp
    ensurenopendingoperation.cpp
    main.cpp
    mainwindow.cpp
    mediawidget.cpp
    osdwidget.cpp
    sqlhelper.cpp
    sqlinterface.cpp)

if(HAVE_DVB)
  set(kaffeinedvb_SRCS
      dvb/dvbcam_linux.cpp
      dvb/dvbchannel.cpp
      dvb/dvbchanneldialog.cpp
      dvb/dvbconfigdialog.cpp
      dvb/dvbdevice.cpp
      dvb/dvbdevice_linux.cpp
      dvb/dvbepg.cpp
      dvb/dvbepgdialog.cpp
      dvb/dvbliveview.cpp
      dvb/dvbmanager.cpp
      dvb/dvbrecording.cpp
      dvb/dvbrecordingdialog.cpp
      dvb/dvbscan.cpp
      dvb/dvbscandialog.cpp
      dvb/dvbsi.cpp
      dvb/dvbtab.cpp
      dvb/dvbtransponder.cpp)
endif(HAVE_DVB)

configure_file(config-kaffeine.h.cmake ${CMAKE_BINARY_DIR}/config-kaffeine.h)

add_executable(kaffeine ${kaffeinedvb_SRCS} ${kaffeine_SRCS})
target_link_libraries(kaffeine Qt5::Sql Qt5::X11Extras KF5::XmlGui KF5::I18n KF5::Solid
		      KF5::KIOCore KF5::KIOFileWidgets KF5::WindowSystem
		      KF5::DBusAddons ${X11_Xscreensaver_LIB} ${VLC_LIBRARY})

if(HAVE_DVB)
    target_link_libraries(kaffeine ${Libdvbv5_LIBRARIES})
endif(HAVE_DVB)

install(TARGETS kaffeine ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES scanfile.dvb DESTINATION ${DATA_INSTALL_DIR}/kaffeine)
install(PROGRAMS org.kde.kaffeine.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES org.kde.kaffeine.appdata.xml DESTINATION ${METAINFODIR})
