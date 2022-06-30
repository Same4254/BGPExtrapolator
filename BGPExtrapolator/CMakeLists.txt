# CMakeList.txt : CMake project for BGPExtrapolator, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

include_directories(${PROJECT_SOURCE_DIR}/BGPExtrapolator/include)
add_executable (BGPExtrapolator "src/Main.cpp"   "src/Util.cpp" "src/Graphs/Graph.cpp" "src/Testing.cpp")

#set(CMAKE_CXX_FLAGS "-fprofile-generate")
#set(CMAKE_CXX_FLAGS "-fprofile-use=*.gcda")

#target_link_libraries(BGPExtrapolator PUBLIC rapidcsv PUBLIC Boost::container PUBLIC Boost::config)

#if ("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
#    target_link_libraries(BGPExtrapolator PUBLIC Shlwapi)
#endif()

#target_link_libraries(BGPExtrapolator PUBLIC rapidcsv PUBLIC benchmark::benchmark)
add_custom_target(copy-test-cases ALL 
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/BGPExtrapolator/TestCases ${PROJECT_BINARY_DIR}/BGPExtrapolator/TestCases
		DEPENDS BGPExtrapolator) 

add_custom_target(copy-default-launch ALL 
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/BGPExtrapolator/DefaultLaunch.json ${PROJECT_BINARY_DIR}/BGPExtrapolator/DefaultLaunch.json
		DEPENDS BGPExtrapolator) 

target_link_libraries(BGPExtrapolator PUBLIC rapidcsv nlohmann_json::nlohmann_json)

install(TARGETS BGPExtrapolator DESTINATION bin)
