link_directories(${CMAKE_BINARY_DIR}/src)

include_directories(${PROJECT_SOURCE_DIR}/src)

add_executable(test main.c)

add_dependencies(test pco)

target_link_libraries(test pco)