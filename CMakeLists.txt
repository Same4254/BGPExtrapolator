# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("BGPExtrapolator")

set(CMAKE_CXX_STANDARD 11)

include(FetchContent)
FetchContent_Declare(
  rapidcsv
  GIT_REPOSITORY "https://github.com/d99kris/rapidcsv.git"
  GIT_TAG        "v8.53"
)
FetchContent_MakeAvailable(rapidcsv)

FetchContent_Declare(
  boost
  GIT_REPOSITORY "https://github.com/boostorg/boost.git"
  GIT_TAG "boost-1.77.0"
)

FetchContent_MakeAvailable(boost)

# Include sub-projects.
add_subdirectory ("BGPExtrapolator")
