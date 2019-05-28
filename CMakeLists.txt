
cmake_minimum_required(VERSION 3.0)

project(BFSS_ALL)

set(CMAKE_CXX_STANDARD 17)

message("CMAKE_BUILD_TYPE   is ${CMAKE_BUILD_TYPE}")
message("CMAKE_C_COMPILER   is ${CMAKE_C_COMPILER}")
message("CMAKE_CXX_COMPILER is ${CMAKE_CXX_COMPILER}")
IF (CMAKE_BUILD_TYPE STREQUAL Debug)
    SET(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -Wno-deprecated -ftemplate-depth-50 -fPIC -pg")
    SET(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -DSN_DEBUG")
    message("CMAKE_CXX_FLAGS_DEBUG    is ${CMAKE_CXX_FLAGS_DEBUG}")
    SET(LINK_FLAGS "${LINK_FLAGS} -pg")
ELSEIF(CMAKE_BUILD_TYPE STREQUAL Release)
    # set(CMAKE_CXX_FLAGS_RELEASE "-O0 -DNDEBUG")  -- valgrind need.
    SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} ${CMAKE_CXX_FLAGS_RELEASE} -Wno-deprecated -ftemplate-depth-50 -finline-functions -fPIC -g")
    message("CMAKE_CXX_FLAGS_RELEASE  is ${CMAKE_CXX_FLAGS_RELEASE}")
    #SET(LINK_FLAGS "${LINK_FLAGS} -pg")
ENDIF()


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/out/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/out/lib)
set(THRIFT_GEN_OUT_DIR ${PROJECT_SOURCE_DIR}/out/thrift)
set(BFSS_UTILS_DIR ${PROJECT_SOURCE_DIR}/utils)
#-------------------

find_package(libmongocxx REQUIRED)
find_package(libbsoncxx REQUIRED)
find_package(liblog4cxx REQUIRED)
find_package(libmemcached REQUIRED)


# google-breakpad
find_path(BREAKPAD_DIR breakpad/client/linux/handler/exception_handler.h)
include_directories(${BREAKPAD_DIR}/breakpad)
add_definitions(-D__STDC_FORMAT_MACROS)
# google-breakpad

# utils
aux_source_directory(${BFSS_UTILS_DIR} UTILS_DIR_SRCS)
add_library(bfss_utils STATIC ${UTILS_DIR_SRCS})
include_directories(${BFSS_UTILS_DIR})
# utils

#------------------- thrift rpc start
function(ADD_THRIFT_SRC name)
    string(TOUPPER ${name} UNAME)
    set(gencpp_SOURCES
            "${THRIFT_GEN_OUT_DIR}/bfss_${name}/BFSS_${UNAME}D.cpp"
            "${THRIFT_GEN_OUT_DIR}/bfss_${name}/bfss_${name}_constants.cpp"
            "${THRIFT_GEN_OUT_DIR}/bfss_${name}/bfss_${name}_types.cpp"
            "${THRIFT_GEN_OUT_DIR}/bfss_${name}/bfss_result_constants.cpp"
            "${THRIFT_GEN_OUT_DIR}/bfss_${name}/bfss_result_types.cpp"
            )

    add_custom_command(OUTPUT ${gencpp_SOURCES}
            COMMAND mkdir -p "${THRIFT_GEN_OUT_DIR}/bfss_${name}"
            COMMAND thrift --gen cpp:no_skeleton -out "${THRIFT_GEN_OUT_DIR}/bfss_${name}" -r "${PROJECT_SOURCE_DIR}/bfss_${name}/bfss_${name}.thrift"
            )

    add_library(bfss_${name} STATIC ${gencpp_SOURCES})
endfunction()

add_thrift_src(regm)
add_thrift_src(sn)
add_thrift_src(api)

include_directories(${THRIFT_GEN_OUT_DIR}/bfss_regm)
include_directories(${THRIFT_GEN_OUT_DIR}/bfss_sn)
include_directories(${THRIFT_GEN_OUT_DIR}/bfss_api)

add_subdirectory(bfss_api)
add_subdirectory(bfss_regm)
add_subdirectory(bfss_sn)
#------------------- thrift rpc end

# unittest(gtest) & go-client
add_subdirectory(TestCase)

