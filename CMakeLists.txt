cmake_minimum_required(VERSION 3.9)

set(CMAKE_C_STANDARD 99)

add_executable(manager manager.c err.c err.h shared_variables.h room.h)
add_executable(player player.c err.c err.h shared_variables.h room.h)


target_link_libraries(manager "-lrt" "-pthread")
target_link_libraries(player "-lrt" "-pthread")

