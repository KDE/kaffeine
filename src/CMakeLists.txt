if(Libdvbv5_FOUND)
  include_directories(${Libdvbv5_INCLUDE_DIRS})
endif(Libdvbv5_FOUND)

qt6_add_resources(RESOURCE_ADDED kaffeine.qrc)

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
      iso-codes.cpp
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
      dvb/dvbtransponder.cpp
      dvb/xmltv.cpp)
endif(HAVE_DVB)

configure_file(config-kaffeine.h.cmake ${CMAKE_BINARY_DIR}/config-kaffeine.h)

add_executable(kaffeine ${kaffeinedvb_SRCS} ${kaffeine_SRCS})
target_link_libraries(kaffeine Qt6::Sql KF6::XmlGui KF6::I18n KF6::Solid
		      KF6::KIOCore KF6::KIOFileWidgets KF6::WindowSystem
		      KF6::DBusAddons ${VLC_LIBRARY})

if(HAVE_DVB)
    target_link_libraries(kaffeine ${Libdvbv5_LIBRARIES})
endif(HAVE_DVB)

install(TARGETS kaffeine ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES scanfile.dvb DESTINATION ${KDE_INSTALL_DATADIR}/kaffeine)
install(PROGRAMS org.kde.kaffeine.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kaffeine.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
