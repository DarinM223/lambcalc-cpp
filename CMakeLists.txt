cmake_minimum_required(VERSION 3.28)
project(lambcalc-cpp)

list(APPEND CMAKE_PREFIX_PATH "${LLMV_DIR}/lib/cmake/llvm/")

find_package(LLVM CONFIG)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -g -fsanitize=address")
set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE SOURCES src/main.cpp src/anf.cpp)

add_executable(lambcalc ${SOURCES})
target_include_directories(lambcalc PRIVATE include/)
target_link_libraries(lambcalc PRIVATE LLVM)
target_compile_features(lambcalc PUBLIC cxx_std_20)