project(BFSS_APID)

include_directories(${LIBMONGOCXX_INCLUDE_DIRS})
include_directories(${LIBBSONCXX_INCLUDE_DIRS})
include_directories(${Log4cxx_INCLUDE_DIRS})
include_directories(${LIBMEMCACHED_INCLUDE_DIR})

aux_source_directory(. DIR_SRCS)

add_executable(BFSS_APID ${DIR_SRCS} main.cpp bfss_api.h bfss_api.cpp)

target_link_libraries(BFSS_APID bfss_api bfss_regm bfss_sn bfss_utils)
target_link_libraries(BFSS_APID thrift)

target_link_libraries(BFSS_APID pthread)

target_link_libraries(BFSS_APID crypto)
target_link_libraries(BFSS_APID config++)

target_link_libraries(BFSS_APID thriftnb)
target_link_libraries(BFSS_APID event)

target_link_libraries(BFSS_APID ${LIBMONGOCXX_LIBRARIES})
target_link_libraries(BFSS_APID ${LIBBSONCXX_LIBRARIES})
target_link_libraries(BFSS_APID ${Log4cxx_LIBRARYS})
target_link_libraries(BFSS_APID ${LIBMEMCACHED_LIBRARY})
target_link_libraries(BFSS_APID boost_serialization)
target_link_libraries(BFSS_APID boost_system)
target_link_libraries(BFSS_APID boost_thread)
target_link_libraries(BFSS_APID breakpad_client)

