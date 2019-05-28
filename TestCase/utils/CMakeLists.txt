
project(Utils_Test)


# Now simply link against gtest or gtest_main as needed. Eg
#add_executable(example example.cpp)
#target_link_libraries(example gtest_main)
#add_test(NAME example_test COMMAND example)
add_executable(Utils_Test Utils_Test.cpp)

target_link_libraries(Utils_Test thrift)
target_link_libraries(Utils_Test crypto)
target_link_libraries(Utils_Test gtest_main)
target_link_libraries(Utils_Test bfss_api bfss_regm bfss_sn bfss_utils)
target_link_libraries(Utils_Test ${LIBMEMCACHED_LIBRARY})
target_link_libraries(Utils_Test boost_serialization)

