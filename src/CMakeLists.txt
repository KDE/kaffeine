set(kaffeine_SRCS
    backend-xine/xinecommands.cpp
    backend-xine/xinemediawidget.cpp
    playlist/playlistmodel.cpp
    playlist/playlisttab.cpp
    datetimeedit.cpp
    dbusobjects.cpp
    main.cpp
    mainwindow.cpp
    mediawidget.cpp
    osdwidget.cpp
    sqlhelper.cpp
    sqltablemodel.cpp)

if(HAVE_DVB)
  set(kaffeinedvb_SRCS
      dvb/dvbcam_linux.cpp
      dvb/dvbchannel.cpp
      dvb/dvbchannelui.cpp
      dvb/dvbconfigdialog.cpp
      dvb/dvbdevice.cpp
      dvb/dvbdevice_linux.cpp
      dvb/dvbepg.cpp
      dvb/dvbliveview.cpp
      dvb/dvbmanager.cpp
      dvb/dvbrecording.cpp
      dvb/dvbscan.cpp
      dvb/dvbscandialog.cpp
      dvb/dvbsi.cpp
      dvb/dvbtab.cpp
      dvb/dvbtransponder.cpp)
endif(HAVE_DVB)

set(kaffeinexbu_SRCS
    backend-xine/xineapplication.cpp
    backend-xine/xinecommands.cpp)

configure_file(config-kaffeine.h.cmake ${CMAKE_BINARY_DIR}/config-kaffeine.h)

kde4_add_executable(kaffeine ${kaffeine_SRCS} ${kaffeinedvb_SRCS})
target_link_libraries(kaffeine ${QT_QTSQL_LIBRARY} ${KDE4_KFILE_LIBS} ${KDE4_KIO_LIBS} ${KDE4_SOLID_LIBS} ${X11_Xscreensaver_LIB})
install(TARGETS kaffeine ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES scanfile.dvb DESTINATION ${DATA_INSTALL_DIR}/kaffeine)
install(FILES kaffeine.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})

kde4_add_executable(kaffeine-xbu ${kaffeinexbu_SRCS})
target_link_libraries(kaffeine-xbu ${X11_X11_LIB} ${XINE_LIBRARY} ${KDE4_KDEUI_LIBS})
install(TARGETS kaffeine-xbu ${INSTALL_TARGETS_DEFAULT_ARGS})
