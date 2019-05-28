#pragma once

#include <BFSS_APID.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/stdcxx.h>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <bfss_utils.h>

#include "gtest/gtest.h"
#include <set>


using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace BFSS_API;

using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;



class BFSS_API_TestBase : public ::testing::Test
{
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    virtual void SetUp();
    virtual void TearDown();

    BFSS_APIDClient * _client;
};



class BFSS_API_Test_Exceptions : public BFSS_API_TestBase
{
public:
    static void SetUpTestCase();
    static void TearDownTestCase() {};
};



inline std::string gen_oid(int oid_len = oid_max_len) {
    char rand_chs[oid_max_len+1] = {0};
    RAND_bytes((unsigned char*)rand_chs, oid_max_len);

    /*oid 有效字符 */
    const char *ch_valid = "-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";

    char *pszY = getenv("GTEST_BFSS_Y");
    if (pszY)   oid_len = std::min(oid_max_len, atoi(pszY));


    for (int i=0; i<oid_len; i++) {
        rand_chs[i] = *(ch_valid + (rand_chs[i] % strlen(ch_valid)));
    }
    rand_chs[oid_len] = 0;

    //std::cout << rand_chs << std::endl;
    return std::string(rand_chs);
}


inline std::string gen_fid()
{
    char str[129];
    srand(time(NULL)+rand());

    sprintf(str, "%x%x-%x-%x-%x-%x%x%x",
            rand(), rand(),                 // Generates a 64-bit Hex number
            rand(),                         // Generates a 32-bit Hex number
            ((rand() & 0x0fff) | 0x4000),   // Generates a 32-bit Hex number of the form 4xxx (4 indicates the UUID version)
            rand() % 0x3fff + 0x8000,       // Generates a 32-bit Hex number in the range [0x8000, 0xbfff]
            rand(), rand(), rand());        // Generates a 96-bit Hex number

    return std::string(str);
}







inline
int encrypt(unsigned char *key, unsigned char *iv,
            const unsigned char *data, int data_len,
            unsigned char *out, int &out_len){
    if (nullptr == key || nullptr == iv || nullptr == data || nullptr == out || data_len < 1) return -1;
    EVP_CIPHER_CTX * ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, iv);
    EVP_EncryptUpdate(ctx, out, &out_len, data, data_len);
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}
inline
int decrypt(unsigned char *key, unsigned char *iv,
            const unsigned char *data, int data_len,
            unsigned char *out, int &out_len){
    if (nullptr == key || nullptr == iv || nullptr == data || nullptr == out || data_len < 1) return -1;
    EVP_CIPHER_CTX * ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, iv);
    EVP_DecryptUpdate(ctx, out, &out_len, data, data_len);
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}
inline
std::string decrypt(const std::string& blkkey, const std::string& in) {
    unsigned char key[32];
    unsigned char iv[32];

    const char *bkey = blkkey.c_str();
    memcpy((void *)key, (void *) bkey,       32);
    memcpy((void *)iv,  (void *)(bkey + 32), 32);

    std::string out; out.resize(in.length(), '\0');
    int out_len = 0;
    decrypt(key, iv, (unsigned char *)in.c_str(), (int)in.length(), (unsigned char *)out.c_str(), out_len);
    return out;
}
inline
std::string encrypt(const std::string& blkkey, const std::string& in) {
    unsigned char key[32];
    unsigned char iv[32];

    const char *bkey = blkkey.c_str();
    memcpy((void *)key, (void *) bkey,       32);
    memcpy((void *)iv,  (void *)(bkey + 32), 32);

    std::string out; out.resize(in.length(), '\0');
    int out_len = 0;
    encrypt(key, iv, (unsigned char *)in.c_str(), (int)in.length(), (unsigned char *)out.c_str(), out_len);
    return out;
}