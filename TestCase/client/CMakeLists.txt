
FIND_LIBRARY(THRIFT_LIBRARY thrift)

add_executable(BFSS_API_Test Client_Test.cpp)

target_link_libraries(BFSS_API_Test ${THRIFT_LIBRARY})
target_link_libraries(BFSS_API_Test gtest_main)
target_link_libraries(BFSS_API_Test bfss_api bfss_regm bfss_sn bfss_utils crypto)


add_executable(BFSS_RegM_Test RegManager_Test.cpp)

target_link_libraries(BFSS_RegM_Test ${THRIFT_LIBRARY})
target_link_libraries(BFSS_RegM_Test gtest_main)
target_link_libraries(BFSS_RegM_Test bfss_regm)


add_executable(BFSS_SN_Test StorageNode_Test.cpp)

target_link_libraries(BFSS_SN_Test ${THRIFT_LIBRARY})
target_link_libraries(BFSS_SN_Test gtest_main)
target_link_libraries(BFSS_SN_Test bfss_regm bfss_sn bfss_utils crypto)



#   #thrift for Go     client/server.
#   cmake_minimum_required(VERSION 3.0)
#   include(go/GolangSimple.cmake)
#
#
#   set(gen_go_SOURCES
#           ${GOPATH}/src/BFSS_REGMD/bfss_regm.go
#           ${GOPATH}/src/BFSS_REGMD/bfss_regm-consts.go
#           ${GOPATH}/src/BFSS_REGMD/GoUnusedProtection__.go
#           ${GOPATH}/src/BFSS_SN/bfss_sn-consts.go
#           ${GOPATH}/src/BFSS_SN/bfss_sn.go
#           ${GOPATH}/src/BFSS_SN/GoUnusedProtection__.go
#           ${GOPATH}/src/BFSS_API/bfss_api.go
#           ${GOPATH}/src/BFSS_API/bfss_api-consts.go
#           ${GOPATH}/src/BFSS_API/GoUnusedProtection__.go
#           )
#
#   message("client -PROJECT_SOURCE_DIR -  ${PROJECT_SOURCE_DIR}")
#   #add_custom_command(OUTPUT bfss_go ${gen_go_SOURCES}
#   add_custom_target(thrift_go
#           COMMAND -rm -rf ${GOPATH}/src/BFSS_API
#           COMMAND -rm -rf ${GOPATH}/src/BFSS_REGM
#           COMMAND -rm -rf ${GOPATH}/src/BFSS_SN
#           COMMAND mkdir -p ${GOPATH}/pkg
#           COMMAND mkdir -p ${GOPATH}/bin
#           COMMAND thrift --gen go -out ${GOPATH}/src -r ${PROJECT_SOURCE_DIR}/../../Service/bfss_api.thrift
#           COMMAND thrift --gen go -out ${GOPATH}/src -r ${PROJECT_SOURCE_DIR}/../../Manage/bfss_regm.thrift
#           COMMAND thrift --gen go -out ${GOPATH}/src -r ${PROJECT_SOURCE_DIR}/../../Storage/bfss_sn.thrift
#           )
#
#   GO_GET(go_redis github.com/hoisie/redis)
#   GO_GET(thrift github.com/apache/thrift/lib/go/thrift/...)
#   ADD_GO_INSTALLABLE_PROGRAM(bfss_go
#           ${PROJECT_SOURCE_DIR}/go/src/bfss_client/bfss_client.go
#           go_redis thrift thrift_go)
#
#   #end of thrift rpc library