cmake_minimum_required(VERSION 3.2)
project(Compiler C CXX)

set(CMAKE_CXX_STANDARD 17)

# Maybe not use glob?
# Own sources
file(GLOB_RECURSE SOURCES src/*.cpp)
# Common sources
file(GLOB_RECURSE COMMON_SOURCES ${Language_SOURCE_DIR}/common/*.cpp)
list(APPEND SOURCES ${COMMON_SOURCES})

# Define executable sources
add_executable(galec ${SOURCES})

# Include catch library 
target_include_directories(galec PRIVATE ${Compiler_SOURCE_DIR}/lib)
# Include own headers
target_include_directories(galec PRIVATE ${Compiler_SOURCE_DIR}/include)
# Include common headers
target_include_directories(galec PRIVATE ${Language_SOURCE_DIR}/common)