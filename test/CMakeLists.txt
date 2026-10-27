# link_directories(${CMAKE_BINARY_DIR}/src)

# list_test
add_executable(list_test list_test.cpp)
add_dependencies(list_test pco)
target_link_libraries(list_test pco pthread GTest::gtest_main)

# klt_test
add_executable(klt_test klt_test.cpp)
add_dependencies(klt_test pco)
target_link_libraries(klt_test pco pthread GTest::gtest_main)

# atomic_test
add_executable(atomic_test atomic_test.cpp)
add_dependencies(atomic_test pco)
target_link_libraries(atomic_test pco pthread GTest::gtest_main)
set_target_properties(atomic_test PROPERTIES COMPILE_FLAGS "-save-temps")

# stdq_test
add_executable(stdq_test lockqueue.cpp)
add_dependencies(stdq_test pco)
target_link_libraries(stdq_test pco pthread)

include(GoogleTest)
gtest_discover_tests(list_test)
gtest_discover_tests(klt_test PROPERTIES TIMEOUT 15)
gtest_discover_tests(atomic_test)

add_executable(queue_test queue_test.c)
target_link_libraries(queue_test pco pthread)
add_test(NAME queue_test COMMAND queue_test)
set_tests_properties(queue_test PROPERTIES TIMEOUT 30)

add_executable(context_test context_test.c)
target_link_libraries(context_test pco m)
add_test(NAME context_test COMMAND context_test)
