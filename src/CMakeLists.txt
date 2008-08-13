set(kaffeine_SRCS
    dvb/dvbchannel.cpp
    dvb/dvbchannelview.cpp
    dvb/dvbconfigdialog.cpp
    dvb/dvbdevice.cpp
    dvb/dvbmanager.cpp
    dvb/dvbscan.cpp
    dvb/dvbscandialog.cpp
    dvb/dvbsi.cpp
    dvb/dvbtab.cpp
    engine.cpp
    engine_phonon.cpp
    kaffeine.cpp
    main.cpp
    manager.cpp
    mediawidget.cpp)

set(kaffeine_UI
    dvb/dvbscandialog.ui)

configure_file(config-kaffeine.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kaffeine.h)
kde4_add_ui_files(kaffeine_SRCS ${kaffeine_UI})
kde4_add_executable(kaffeine4 ${kaffeine_SRCS})
target_link_libraries(kaffeine4 ${KDE4_KIO_LIBS} ${KDE4_PHONON_LIBS})

install(TARGETS kaffeine4 DESTINATION ${BIN_INSTALL_DIR})
install(FILES kaffeine4ui.rc scanfile_g.dvb DESTINATION ${DATA_INSTALL_DIR}/kaffeine4)
install (FILES kaffeine4.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
