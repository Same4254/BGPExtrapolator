# CMakeList.txt : CMake project for BGPExtrapolator, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

include_directories(${PROJECT_SOURCE_DIR}/BGPExtrapolator/include)
add_executable (BGPExtrapolator "src/Main.cpp"   "src/Util.cpp" "src/Graphs/Graph.cpp")

#target_link_libraries(BGPExtrapolator PUBLIC rapidcsv PUBLIC Boost::container PUBLIC Boost::config)

#if ("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
#    target_link_libraries(BGPExtrapolator PUBLIC Shlwapi)
#endif()

#target_link_libraries(BGPExtrapolator PUBLIC rapidcsv PUBLIC benchmark::benchmark)
add_custom_target(copy-test-cases ALL 
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/BGPExtrapolator/TestCases ${PROJECT_BINARY_DIR}/BGPExtrapolator/TestCases
		DEPENDS BGPExtrapolator) 
target_link_libraries(BGPExtrapolator PUBLIC rapidcsv)

install(TARGETS BGPExtrapolator DESTINATION bin)
