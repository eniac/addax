CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

INCLUDE_DIRECTORIES(../addax-lib)

add_definitions(-std=c++11)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_EXECUTABLE(share-commit-gen share-commit-gen.cpp)
TARGET_LINK_LIBRARIES(share-commit-gen m ssl crypto pthread)
