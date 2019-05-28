
project(BFSS_REGMD)

include_directories(${LIBMONGOCXX_INCLUDE_DIRS})
include_directories(${LIBBSONCXX_INCLUDE_DIRS})
include_directories(${Log4cxx_INCLUDE_DIRS})

aux_source_directory(. DIR_SRCS)

add_executable(BFSS_REGMD ${DIR_SRCS})

target_link_libraries(BFSS_REGMD bfss_regm bfss_utils)

target_link_libraries(BFSS_REGMD thrift)

target_link_libraries(BFSS_REGMD pthread)

target_link_libraries(BFSS_REGMD crypto)

target_link_libraries(BFSS_REGMD config++)

target_link_libraries(BFSS_REGMD thrift)
target_link_libraries(BFSS_REGMD thriftnb)
target_link_libraries(BFSS_REGMD boost_system)
target_link_libraries(BFSS_REGMD event)

target_link_libraries(BFSS_REGMD ${LIBMONGOCXX_LIBRARIES})
target_link_libraries(BFSS_REGMD ${LIBBSONCXX_LIBRARIES})
target_link_libraries(BFSS_REGMD ${Log4cxx_LIBRARYS})

target_link_libraries(BFSS_REGMD breakpad_client)
