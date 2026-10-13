cmake_minimum_required(VERSION 3.20)
project(hydroacoustic LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Единая папка для FetchContent
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/vendor)
include(FetchContent)

add_subdirectory(common)
add_subdirectory(simulator)
# add_subdirectory(model)   # включим, когда дойдём до LibTorch