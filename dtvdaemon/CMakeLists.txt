set(dtvdaemon_SRCS
    connection.cpp
    dtvdaemon.cpp
    log.cpp
    main.cpp)

kde4_add_executable(dtvdaemon ${dtvdaemon_SRCS})
target_link_libraries(dtvdaemon ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY})
install(TARGETS dtvdaemon ${INSTALL_TARGETS_DEFAULT_ARGS})
