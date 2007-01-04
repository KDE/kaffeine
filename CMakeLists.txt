project(kaffeine)

set(kaffeine_SRCS
    kaffeine.cpp
    main.cpp)

kde4_automoc(${kaffeine_SRCS})

kde4_add_executable(kaffeine ${kaffeine_SRCS})

target_link_libraries(kaffeine ${KDE4_KDEUI_LIBS})

install(TARGETS kaffeine DESTINATION ${BIN_INSTALL_DIR})

install(FILES kaffeineui.rc DESTINATION ${DATA_INSTALL_DIR}/kaffeine)
