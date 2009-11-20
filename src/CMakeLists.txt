set(kaffeine_SRCS
    dvb/dvbchannel.cpp
    dvb/dvbchannelui.cpp
    dvb/dvbconfigdialog.cpp
    dvb/dvbdevice.cpp
    dvb/dvbepg.cpp
    dvb/dvbliveview.cpp
    dvb/dvbmanager.cpp
    dvb/dvbrecording.cpp
    dvb/dvbscan.cpp
    dvb/dvbscandialog.cpp
    dvb/dvbsi.cpp
    dvb/dvbtab.cpp
    playlist/playlistmodel.cpp
    playlist/playlisttab.cpp
    datetimeedit.cpp
    dbusobjects.cpp
    kaffeine.cpp
    main.cpp
    mediawidget.cpp
    osdwidget.cpp
    proxytreeview.cpp
    sqlhelper.cpp
    tablemodel.cpp)

set(kaffeinedvbmodule_SRCS
    dvb/dvbdevice_linux.cpp)

set(kaffeinedvbdebugmodule_SRCS
    dvb/dvbdevice_debug.cpp)

configure_file(config-kaffeine.h.cmake ${CMAKE_BINARY_DIR}/config-kaffeine.h)

kde4_add_executable(kaffeine ${kaffeine_SRCS})
target_link_libraries(kaffeine ${QT_QTSQL_LIBRARY} ${KDE4_KFILE_LIBS} ${KDE4_KIO_LIBS} ${KDE4_PHONON_LIBS} ${KDE4_SOLID_LIBS})
install(TARGETS kaffeine ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES scanfile.dvb DESTINATION ${DATA_INSTALL_DIR}/kaffeine)
install(FILES kaffeine.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})

if(NOT WIN32)
kde4_add_plugin(kaffeinedvb ${kaffeinedvbmodule_SRCS})
target_link_libraries(kaffeinedvb ${KDE4_KDECORE_LIBRARY} ${KDE4_SOLID_LIBS})
install(TARGETS kaffeinedvb DESTINATION ${PLUGIN_INSTALL_DIR})
endif(NOT WIN32)

if(BUILD_DEBUG_MODULE)
  kde4_add_plugin(kaffeinedvbdebug ${kaffeinedvbdebugmodule_SRCS})
  target_link_libraries(kaffeinedvbdebug ${KDE4_KDECORE_LIBRARY})
  install(TARGETS kaffeinedvbdebug DESTINATION ${PLUGIN_INSTALL_DIR})
endif(BUILD_DEBUG_MODULE)
