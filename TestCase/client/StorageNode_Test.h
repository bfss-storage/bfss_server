#pragma once

#include <BFSS_SND.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/stdcxx.h>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>


#include "gtest/gtest.h"
#include <set>
#include <bfss_utils.h>


using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace BFSS_SN;

using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;



struct object_cache__sn_ {
    int32_t VolumeId = 0;
    int32_t BeginIndex = 0;
    int32_t BeginOffset = 0;
    int32_t Size = 0;

    std::string ctx;
    object_cache__sn_() : ctx(sizeof(bfss::oid_blk_info), '\0') {}
};

inline void build_cache_context__sn_(object_cache__sn_ &_cache) {
    bfss::oid_blk_info *_fbi = (bfss::oid_blk_info *) _cache.ctx.c_str();
    _fbi->BeginIndex = _cache.BeginIndex;
    _fbi->BeginOffset = _cache.BeginOffset;
    _fbi->Size = _cache.Size;
}



class BFSS_SN_TestClient : public ::testing::Test
{
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

protected:
    virtual void SetUp();
    virtual void TearDown();


    BFSS_SNDClient * _client;
};

inline std::string gen_fid()
{
    srand(time(NULL)+rand());

    char str[129] = {0};
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
    EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key, iv);
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
    EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key, iv);
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