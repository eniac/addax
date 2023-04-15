CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

INCLUDE_DIRECTORIES(../addax-lib)

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(server-interactive server-interactive.cpp)
TARGET_LINK_LIBRARIES(server-interactive m ssl crypto pthread)

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(bidder-interactive bidder-interactive.cpp)
TARGET_LINK_LIBRARIES(bidder-interactive m ssl crypto pthread)

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(publisher-interactive publisher-interactive.cpp)
TARGET_LINK_LIBRARIES(publisher-interactive m ssl crypto pthread)
