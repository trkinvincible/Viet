cmake_minimum_required(VERSION 3.10)
project(synopsys)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_FLAGS "-pg") - use only with GCC 's own profiler

#list (APPEND CMAKE_P.REFIX_PATH "/media/trk/DATA/Code-Factory/NUTO/externals_libs")

SET( _SOURCES_
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

add_executable(${PROJECT_NAME} ${_SOURCES_})

target_link_libraries(${PROJECT_NAME} -pthread)
