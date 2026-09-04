cmake_minimum_required(VERSION 3.16)
project(bookkeeping_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -march=native -DNDEBUG")

add_library(books INTERFACE)
target_include_directories(books INTERFACE include)
