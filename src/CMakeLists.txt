set(kaffeine_SRCS
    dvb/dvbchannel.cpp
    dvb/dvbchannelui.cpp
    dvb/dvbconfigdialog.cpp
    dvb/dvbdevice.cpp
    dvb/dvbepg.cpp
    dvb/dvbmanager.cpp
    dvb/dvbrecording.cpp
    dvb/dvbscan.cpp
    dvb/dvbscandialog.cpp
    dvb/dvbsi.cpp
    dvb/dvbtab.cpp
    playlist/playlistmodel.cpp
    playlist/playlisttab.cpp
    kaffeine.cpp
    main.cpp
    mediawidget.cpp
    osdwidget.cpp
    proxytreeview.cpp)

set(kaffeinedvbmodule_SRCS
    dvb/dvbdevice_linux.cpp)

kde4_add_executable(kaffeine ${kaffeine_SRCS})
target_link_libraries(kaffeine ${KDE4_KFILE_LIBS} ${KDE4_KIO_LIBS} ${KDE4_PHONON_LIBS})

kde4_add_plugin(kaffeinedvb ${kaffeinedvbmodule_SRCS})
target_link_libraries(kaffeinedvb ${KDE4_KDECORE_LIBRARY} ${KDE4_SOLID_LIBS})

install(TARGETS kaffeine ${INSTALL_TARGETS_DEFAULT_ARGS})
install(TARGETS kaffeinedvb DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES scanfile.dvb DESTINATION ${DATA_INSTALL_DIR}/kaffeine)
install(FILES kaffeine.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
