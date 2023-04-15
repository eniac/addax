CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

INCLUDE_DIRECTORIES(../addax-lib)

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(auction-non-interactive auction-non-interactive.cpp)
TARGET_LINK_LIBRARIES(auction-non-interactive m ssl crypto pthread)

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(auction-interactive auction-interactive.cpp)
TARGET_LINK_LIBRARIES(auction-interactive m ssl crypto pthread)
