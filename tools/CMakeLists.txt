add_executable(convertscanfiles convertscanfiles.cpp ../src/dvb/dvbtransponder.cpp)
add_executable(updatedvbsi updatedvbsi.cpp)
add_executable(updatemimetypes updatemimetypes.cpp)
add_executable(updatesource updatesource.cpp)

target_link_libraries(convertscanfiles Qt5::Core)
target_link_libraries(updatedvbsi Qt5::Core Qt5::Xml)
target_link_libraries(updatemimetypes Qt5::Core)
target_link_libraries(updatesource Qt5::Core)
