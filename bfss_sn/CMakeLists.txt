project(BFSS_SND)

include_directories(${LIBMONGOCXX_INCLUDE_DIRS})
include_directories(${LIBBSONCXX_INCLUDE_DIRS})
include_directories(${Log4cxx_INCLUDE_DIRS})

aux_source_directory(. DIR_SRCS)

add_executable(BFSS_SND ${DIR_SRCS})

target_link_libraries(BFSS_SND thrift)

target_link_libraries(BFSS_SND bfss_sn bfss_regm bfss_utils)

target_link_libraries(BFSS_SND pthread)

target_link_libraries(BFSS_SND crypto)
target_link_libraries(BFSS_SND config++)

target_link_libraries(BFSS_SND thrift)
target_link_libraries(BFSS_SND thriftnb)
target_link_libraries(BFSS_SND boost_system)
target_link_libraries(BFSS_SND boost_thread)
target_link_libraries(BFSS_SND event)

target_link_libraries(BFSS_SND ${LIBMONGOCXX_LIBRARIES})
target_link_libraries(BFSS_SND ${LIBBSONCXX_LIBRARIES})
target_link_libraries(BFSS_SND ${Log4cxx_LIBRARYS})

target_link_libraries(BFSS_SND breakpad_client)
