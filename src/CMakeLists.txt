file(GLOB_RECURSE PCO_SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.c)

if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(ASM_FILES ./context/context_x86.asm)
    else()
        set(ASM_FILES ./context/context_x64.asm)
    endif()
    set_property(SOURCE ${ASM_FILES} PROPERTY LANGUAGE ASM_MASM)
else()
    set(ASM_FILES ./context/context.S ./sigctx_x64.S ./atomic_x64.S)
endif()
list(APPEND PCO_SRC_FILES ${ASM_FILES})

message(${PCO_SRC_FILES})

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(pco ${PCO_SRC_FILES})
target_link_libraries(pco pthread rt)
