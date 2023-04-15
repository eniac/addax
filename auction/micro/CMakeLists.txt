CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

INCLUDE_DIRECTORIES(../addax-lib)

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(adv-cost adv-cost.cpp)
TARGET_LINK_LIBRARIES(adv-cost m ssl crypto pthread)
