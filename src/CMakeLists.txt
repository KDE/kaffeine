set(kaffeine_SRCS
    dvb/dvbdevice.cpp
    dvb/dvbtab.cpp
    kaffeine.cpp
    main.cpp
    manager.cpp
    mediawidget.cpp)

configure_file(config-kaffeine.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kaffeine.h)
kde4_add_executable(kaffeine ${kaffeine_SRCS})
target_link_libraries(kaffeine ${KDE4_KIO_LIBS} ${KDE4_PHONON_LIBS})

install(TARGETS kaffeine DESTINATION ${BIN_INSTALL_DIR})
install(FILES kaffeineui.rc DESTINATION ${DATA_INSTALL_DIR}/kaffeine)
