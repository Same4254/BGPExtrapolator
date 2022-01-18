# CMakeList.txt : CMake project for BGPExtrapolator, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

add_executable (BGPExtrapolator "Main.cpp" "BGPExtrapolator.h"      "ProcessAnnouncement.cpp" "Graph.cpp" "Util.cpp")

#target_link_libraries(BGPExtrapolator PUBLIC rapidcsv PUBLIC Boost::container PUBLIC Boost::config)

#if ("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
#    target_link_libraries(BGPExtrapolator PUBLIC Shlwapi)
#endif()

#target_link_libraries(BGPExtrapolator PUBLIC rapidcsv PUBLIC benchmark::benchmark)

target_link_libraries(BGPExtrapolator PUBLIC rapidcsv)

install(TARGETS BGPExtrapolator DESTINATION bin)