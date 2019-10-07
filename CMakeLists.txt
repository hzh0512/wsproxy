cmake_minimum_required(VERSION 3.15)
project(wsproxy)

set(CMAKE_CXX_STANDARD 11)

add_executable(wspclient client.cpp Connection.cpp)
add_executable(wspserver server.cpp Connection.cpp)

find_package(websocketpp)
if (WEBSOCKETPP_FOUND)
    include_directories(${WEBSOCKETPP_INCLUDE_DIR})
else ()
    message (FATAL_ERROR "Failed to find required dependency: websocketpp")
endif()

find_package (Boost 1.39.0 COMPONENTS system thread)
if (Boost_FOUND)
    include_directories (${Boost_INCLUDE_DIRS})
    link_directories (${Boost_LIBRARY_DIRS})
else ()
    message (FATAL_ERROR "Failed to find required dependency: boost")
endif ()

target_link_libraries(wspclient pthread rt boost_system)
target_link_libraries(wspserver pthread rt boost_system)
