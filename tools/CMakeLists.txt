add_executable(convertscanfiles convertscanfiles.cpp ../src/dvb/dvbtransponder.cpp)
add_executable(updatedvbsi updatedvbsi.cpp)
add_executable(updatemimetypes updatemimetypes.cpp)
add_executable(updatesource updatesource.cpp)

target_link_libraries(convertscanfiles Qt6::Core)
target_link_libraries(updatedvbsi Qt6::Core Qt6::Xml)
target_link_libraries(updatemimetypes Qt6::Core)
target_link_libraries(updatesource Qt6::Core)
