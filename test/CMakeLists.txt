# link_directories(${CMAKE_BINARY_DIR}/src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -Wno-error=deprecated-copy")

add_executable(pco_test test.c)

add_dependencies(pco_test pco)

target_link_libraries(pco_test pco pthread)

# include_directories(include)

add_executable(stdq_test lockqueue.cpp)
add_dependencies(stdq_test pco)

target_link_libraries(stdq_test pco pthread)