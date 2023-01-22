cmake_minimum_required(VERSION 3.10)
project(pco)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")

set(CMAKE_C_FLAGS "-fverbose-asm ${CMAKE_C_FLAGS}")

if(MSVC)
    enable_language(C CXX ASM_MASM)
else()
    enable_language(C CXX ASM)
endif()

message(${CMAKE_BINARY_DIR}/src)

include_directories(include)

add_subdirectory(src)

add_subdirectory(test)

set_target_properties(pco PROPERTIES COMPILE_FLAGS "-save-temps")
set_target_properties(pco_test PROPERTIES COMPILE_FLAGS "-save-temps")