cmake_minimum_required(VERSION 3.10)
project(pco)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -Wno-error=deprecated-copy")

set(CMAKE_C_FLAGS "-fverbose-asm ${CMAKE_C_FLAGS}")

if(MSVC)
    enable_language(C CXX ASM_MASM)
else()
    enable_language(C CXX ASM)
endif()

message(${CMAKE_BINARY_DIR}/src)

# GTest
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY    https://github.com/google/googletest.git
  GIT_TAG           b796f7d44681514f58a683a3a71ff17c94edb0c1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(include)

add_subdirectory(src)

add_subdirectory(test)

set_target_properties(pco PROPERTIES COMPILE_FLAGS "-save-temps")
set_target_properties(pco_test PROPERTIES COMPILE_FLAGS "-save-temps")