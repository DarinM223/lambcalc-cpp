cmake_minimum_required(VERSION 3.28)
project(lambcalc-cpp)

list(APPEND CMAKE_PREFIX_PATH "${LLVM_DIR}/lib/cmake/llvm/")

find_package(LLVM CONFIG)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

include(FetchContent)

# Downloads googletest from external git repository.
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic -g -fsanitize=address,leak,undefined")
set(CMAKE_CXX_STANDARD 23)

include_directories(include/)

file(
    GLOB_RECURSE SOURCES
    src/ast.cpp
    src/anf.cpp
    src/visitor.cpp
    src/convert.cpp
    src/hoist.cpp
    src/lower.cpp
    src/lexer.cpp
    src/parser.cpp
    src/rename.cpp
)
file(
    GLOB_RECURSE TEST_SOURCES
    test/anf.cpp
    test/convert.cpp
    test/hoist.cpp
    test/lower.cpp
    test/lexer.cpp
    test/parser.cpp
    test/rename.cpp
    test/arena.cpp
)

add_library(lambcalc-lib ${SOURCES})
target_compile_features(lambcalc-lib PUBLIC cxx_std_23)

add_executable(lambcalc src/main.cpp)
target_link_libraries(lambcalc PRIVATE LLVM lambcalc-lib)
target_compile_features(lambcalc PUBLIC cxx_std_23)

enable_testing()

add_executable(lambcalc-test ${TEST_SOURCES})
target_link_libraries(lambcalc-test PRIVATE LLVM lambcalc-lib gtest_main gmock_main)
target_compile_features(lambcalc-test PUBLIC cxx_std_23)

include(GoogleTest)
gtest_discover_tests(lambcalc-test)