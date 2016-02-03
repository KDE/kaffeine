set(dtvdaemon_SRCS
    connection.cpp
    dtvdaemon.cpp
    log.cpp
    main.cpp)

add_executable(dtvdaemon ${dtvdaemon_SRCS})
target_link_libraries(dtvdaemon Qt5::Core Qt5::Network)
install(TARGETS dtvdaemon ${INSTALL_TARGETS_DEFAULT_ARGS})
