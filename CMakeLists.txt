cmake_minimum_required(VERSION 3.0)
project(tairgis_module)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -W -Wall -g -ggdb -std=c99 -O2 -Wno-strict-aliasing -Wno-typedef-redefinition -Wno-sign-compare -Wno-unused-parameter")
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    if (GCOV_MODE) 
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    endif()
endif ()

add_subdirectory(src)
