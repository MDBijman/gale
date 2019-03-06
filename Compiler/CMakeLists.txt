cmake_minimum_required(VERSION 3.2)
project(Compiler C CXX)
include(CheckLanguage)

set(CMAKE_CXX_STANDARD 17)

# Maybe not use glob?
file(GLOB_RECURSE SOURCES src/*.cpp)

check_language(ASM_NASM)
check_language(ASM_MASM)

if(CMAKE_ASM_NASM_COMPILER)
    enable_language(ASM_NASM)
    list(APPEND SOURCES "src/fe/pipeline/vm_nasm.asm")
elseif(CMAKE_ASM_MASM_COMPILER)
    enable_language(ASM_MASM)
    list(APPEND SOURCES "src/fe/pipeline/vm_masm.asm")
else()
    message(FATAL_ERROR "No suitable asm compiler found")
endif()

add_executable(Compiler ${SOURCES})

target_include_directories(Compiler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_include_directories(Compiler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)