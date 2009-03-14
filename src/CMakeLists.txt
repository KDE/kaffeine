set(kaffeine_SRCS
    dvb/dvbchannel.cpp
    dvb/dvbchannelview.cpp
    dvb/dvbconfigdialog.cpp
    dvb/dvbdevice.cpp
    dvb/dvbmanager.cpp
    dvb/dvbrecording.cpp
    dvb/dvbscan.cpp
    dvb/dvbscandialog.cpp
    dvb/dvbsi.cpp
    dvb/dvbtab.cpp
    playlist/playlisttab.cpp
    kaffeine.cpp
    main.cpp
    mediawidget.cpp
    proxytreeview.cpp)

set(kaffeine_UI
    dvb/dvbscandialog.ui)

kde4_add_ui_files(kaffeine_SRCS ${kaffeine_UI})
kde4_add_executable(kaffeine4 ${kaffeine_SRCS})
target_link_libraries(kaffeine4 ${KDE4_KIO_LIBS} ${KDE4_PHONON_LIBS} ${KDE4_SOLID_LIBS})

install(TARGETS kaffeine4 ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES scanfile_g.dvb DESTINATION ${DATA_INSTALL_DIR}/kaffeine4)
install(FILES kaffeine4.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
