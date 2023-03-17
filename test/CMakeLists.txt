# link_directories(${CMAKE_BINARY_DIR}/src)

enable_testing()

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

# pco_test
add_executable(pco_test test.c)
add_dependencies(pco_test pco)
target_link_libraries(pco_test pco pthread)

# stdq_test
add_executable(stdq_test lockqueue.cpp)
add_dependencies(stdq_test pco)
target_link_libraries(stdq_test pco pthread)

include(GoogleTest)
gtest_discover_tests(list_test klt_test atomic_test)