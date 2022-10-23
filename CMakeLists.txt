cmake_minimum_required(VERSION 3.10)
project(pco)
set(C_STANDARD 11)

if(MSVC)
    enable_language(C ASM_MASM)
else()
    enable_language(C ASM)
endif()

message(${CMAKE_BINARY_DIR}/src)

include_directories(include)

add_subdirectory(src)

add_subdirectory(test)
