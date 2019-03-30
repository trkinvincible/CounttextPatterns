project(groundlabs)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_STANDARD 14)

SET( _SOURCES_

    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

SET( _HEADER_

    ${CMAKE_CURRENT_SOURCE_DIR}/utility.h
    ${CMAKE_CURRENT_SOURCE_DIR}/action_manager.h
)

######################
#Include Definitions #
######################
add_definitions(-DUSING_BOOST_IPC)
add_definitions(-DUSING_LOCK_FREE_CODE)

######################
#Include Directories #
######################
include_directories()

######################
#link Directories    #
######################
link_directories()

######################
# Create executable  #
######################
add_executable(${PROJECT_NAME} ${_SOURCES_} ${_HEADER_})

target_link_libraries(${PROJECT_NAME}
    -lpthread
    #-lboost_iostreams
)

######################
#      Install       #
######################
