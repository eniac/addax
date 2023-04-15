CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

INCLUDE_DIRECTORIES("../addax-lib")

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(plain-client plain-client.cpp)
TARGET_LINK_LIBRARIES(plain-client m ssl crypto pthread)

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(plain-server plain-server.cpp)
TARGET_LINK_LIBRARIES(plain-server m ssl crypto pthread)

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(addax-client addax-client.cpp)
TARGET_LINK_LIBRARIES(addax-client m ssl crypto pthread)

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(addax-server addax-server.cpp)
TARGET_LINK_LIBRARIES(addax-server m ssl crypto pthread)