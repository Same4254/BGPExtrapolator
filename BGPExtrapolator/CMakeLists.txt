# CMakeList.txt : CMake project for BGPExtrapolator, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

add_executable (BGPExtrapolator "Main.cpp" "BGPExtrapolator.h"      "ProcessAnnouncement.cpp" "Graph.cpp" "Util.cpp")

target_link_libraries(BGPExtrapolator PUBLIC rapidcsv PUBLIC Boost::container PUBLIC Boost::config)

install(TARGETS BGPExtrapolator DESTINATION bin)