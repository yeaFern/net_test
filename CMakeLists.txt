cmake_minimum_required(VERSION 2.8.12)
project(net_test)

file(GLOB_RECURSE SERVER_SRC
    server/*.cpp
    server/*.h
    shared/*.cpp
    shared/*.h
    deps/enet/enet.c
    deps/enet/enet.h
)

file(GLOB_RECURSE CLIENT_SRC
    client/*.cpp
    client/*.h
    shared/*.cpp
    shared/*.h
    deps/enet/enet.c
    deps/enet/enet.h
    deps/olcPixelGameEngine/*.cpp
    deps/olcPixelGameEngine/*.h
)

foreach(_source IN ITEMS ${SERVER_SRC})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_group_path}")
    source_group("${_group_path}" FILES "${_source}")
endforeach()

foreach(_source IN ITEMS ${CLIENT_SRC})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "${CMAKE_SOURCE_DIR}" "" _group_path "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_group_path}")
    source_group("${_group_path}" FILES "${_source}")
endforeach()

set(CMAKE_CXX_STANDARD 17)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT server)

add_executable(server ${SERVER_SRC})
target_include_directories(server PRIVATE deps/enet)
target_include_directories(server PRIVATE shared)

add_executable(client ${CLIENT_SRC})
set_property(TARGET client PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$(ProjectDir)/../")
target_include_directories(client PRIVATE deps/olcPixelGameEngine)
target_include_directories(client PRIVATE deps/enet)
target_include_directories(client PRIVATE shared)
