//
// Created by han on 19-2-9.
//

#ifndef BFSS_ALL_UTILS_TEST_H
#define BFSS_ALL_UTILS_TEST_H

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/stdcxx.h>


#include "gtest/gtest.h"
#include <set>


using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;


#include <bfss_utils.h>

#include <openssl/rand.h>
#include <openssl/err.h>
inline std::string gen_oid()
{
    char rand_chs[oid_max_len*2] = {0};
    RAND_bytes((unsigned char*)rand_chs, oid_max_len*2);

    /*oid 有效字符 */
    const char *ch_valid = "-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";

    srand(time(NULL)+rand());
    int rand_len = rand() % oid_max_len + 1;

    for (int i=0; i<rand_len; i++) {
        rand_chs[i] = *(ch_valid + (rand_chs[i] % strlen(ch_valid)));
    }
    rand_chs[rand_len] = 0; /*cut off the rand_chs*/

    //std::cout << rand_chs << std::endl;
    return std::string(rand_chs);
}


#endif //BFSS_ALL_UTILS_TEST_H
