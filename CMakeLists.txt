cmake_minimum_required(VERSION 3.28)
project(lambcalc-cpp)

list(APPEND CMAKE_PREFIX_PATH "${LLVM_DIR}/lib/cmake/llvm/")

find_package(LLVM CONFIG)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

include(ExternalProject)

# Downloads googletest from external git repository.
ExternalProject_Add(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG main
    CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
)

# Get some properties from the googletest project.
ExternalProject_Get_Property(googletest source_dir binary_dir)

# Sets up googletest library.
add_library(libgtest UNKNOWN IMPORTED)
set_target_properties(libgtest PROPERTIES
    "IMPORTED_LOCATION" "${binary_dir}/lib/libgtest.a"
    "IMPORTED_LINK_INTERFACE_LIBRARIES" "${CMAKE_THREAD_LIBS_INIT}"
)
add_dependencies(libgtest googletest)

# Sets up googlemock library.
add_library(libgmock UNKNOWN IMPORTED)
set_target_properties(libgmock PROPERTIES
    "IMPORTED_LOCATION" "${binary_dir}/lib/libgmock.a"
    "IMPORTED_LINK_INTERFACE_LIBRARIES" "${CMAKE_THREAD_LIBS_INIT}"
)
add_dependencies(libgmock googletest)

# Include the googletest and googlemock libraries along with your main source folder.
include_directories(
    "${source_dir}/googletest/include"
    "${source_dir}/googlemock/include"
    ${CMAKE_SOURCE_DIR}/src
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -g -fsanitize=address,leak,undefined")
set(CMAKE_CXX_STANDARD 23)

include_directories(include/)

file(
    GLOB_RECURSE SOURCES
    src/main.cpp
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
    test/main.cpp
    test/anf.cpp
    test/convert.cpp
    test/hoist.cpp
    test/lower.cpp
    test/lexer.cpp
    test/parser.cpp
    test/rename.cpp
)

add_executable(lambcalc ${SOURCES})
target_link_libraries(lambcalc PRIVATE LLVM)
target_compile_features(lambcalc PUBLIC cxx_std_23)

add_library(lambcalc-lib ${SOURCES})
target_compile_features(lambcalc-lib PUBLIC cxx_std_23)

add_executable(lambcalc-test ${TEST_SOURCES})
add_dependencies(lambcalc-test libgtest libgmock)
target_link_libraries(lambcalc-test PRIVATE LLVM lambcalc-lib libgtest libgmock)
target_compile_features(lambcalc-test PUBLIC cxx_std_23)