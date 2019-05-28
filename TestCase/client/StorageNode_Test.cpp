

#include "StorageNode_Test.h"
#include "bfss_utils.h"
#include <bfss_cache.h>

using namespace bfss;


static std::map<std::string, object_cache__sn_> _fid_cache_map;

void BFSS_SN_TestClient::SetUpTestCase()
{
    //  启动服务
    _fid_cache_map.clear();
    /* 环境变量控制运行时参数
export GTEST_BFSS_Z=26
export GTEST_BFSS_X=16
export GTEST_BFSS_S=400
export GTEST_BFSS_R=1024
export GTEST_BFSS_B=1024    #Begin-Block
     ================== */
    char *pszZ = getenv("GTEST_BFSS_Z");/* limit the   file's max size ->  2^z */
    char *pszX = getenv("GTEST_BFSS_X");/* limit the random's max size ->  2^x */
    char *pszS = getenv("GTEST_BFSS_S");/* limit the  total's max size ->  S MBytes */
    char *pszR = getenv("GTEST_BFSS_R");/* limit the  for     max size ->  1024 */
    char *pszB = getenv("GTEST_BFSS_B");/* limit the  for     max size ->  1024 */

    // 初始化文件信息   只确定文件大小 文件内容根据大小临时构造
    int z = 24;  if (pszZ) z = atoi(pszZ);/* limit the   file's max size ->  2^z */
    int x = z-4; if (pszX) x = atoi(pszX);/* limit the random's max size ->  2^x */
    int s = 300; if (pszS) s = atoi(pszS);/* limit the  total's max size ->  S MBytes */
    int r = 1024;if (pszR) r = atoi(pszR);/* limit the  for     max size ->  1024 */
    int b = 0;   if (pszB) b = atoi(pszB);/* limit the  for     max size ->  1024 */

    std::cout << "export GTEST_BFSS_Z=" << z << std::endl;
    std::cout << "export GTEST_BFSS_X=" << x << std::endl;
    std::cout << "export GTEST_BFSS_S=" << s << std::endl;
    std::cout << "export GTEST_BFSS_R=" << r << std::endl;
    std::cout << "export GTEST_BFSS_B=" << b << std::endl;
    std::string ip("127.0.0.1");    char *pszIP = getenv("GTEST_BFSS_SN_IP");    if (pszIP) { ip = std::string(pszIP);}
    std::cout << "export GTEST_BFSS_SN_IP=" << ip << std::endl;
    int port = 9091;                char *pszPORT = getenv("GTEST_BFSS_SN_PORT");if (pszPORT) { port = atoi(pszPORT);}
    std::cout << "export GTEST_BFSS_SN_PORT=" << port << std::endl;

    int32_t size = 0;
    int64_t offset = 0;
    for (int i=0; i<r; i++) {

        if (i < z) {
            size = 1 << i;         // 指数递增的文件大小
        } else {
            size = rand() % (1<<x) + 3; // 随机文件大小
        }

        std::string fid = gen_fid();
        object_cache__sn_& cache = _fid_cache_map[fid];

        cache.VolumeId = 10002;
        cache.BeginIndex = (int32_t)(offset /blk_size) + b;
        cache.BeginOffset = (int32_t)(offset%blk_size);
        cache.Size = size;
        build_cache_context__sn_(cache);

        offset += size;             /* 在磁盘上的偏移 */
        offset += 16 - (size%16);   /* 必须16字节对齐 */

        if (offset > ((int64_t)s)*_1M_SIZE) {
            break;
        }

        // std::cout << i << "-   fid=>" << fid << " size " << size /*<< std::endl*/;
        // std::cout << "  BeginIndex=>" << cache.BeginIndex << "  BeginOffset=>" << cache.BeginOffset << std::endl;
    }

    EXPECT_TRUE(_fid_cache_map.size() > 0);
    std::cout << "cache.BeginIndex -------- from " << b << " to " << b + (offset / blk_size) << std::endl;
    std::cout << "setup ------------------------------- test -" <<  _fid_cache_map.size()  << "- file objects--." << std::endl;
}

void BFSS_SN_TestClient::TearDownTestCase()
{

    // 停止服务
}

void BFSS_SN_TestClient::SetUp()
{
/* 环境变量控制运行时参数
export GTEST_BFSS_SN_IP=127.0.0.1
export GTEST_BFSS_SN_PORT=9092
 */
    std::string ip("127.0.0.1");
    char *pszIP = getenv("GTEST_BFSS_SN_IP");
    if (pszIP)  ip = std::string(pszIP);

    int port = 9091;
    char *pszPORT = getenv("GTEST_BFSS_SN_PORT");
    if (pszPORT)  port = atoi(pszPORT);

    stdcxx::shared_ptr<TTransport> socket(new TSocket(ip.c_str(), port));
    stdcxx::shared_ptr<TTransport> transport(new TFramedTransport(socket));
    stdcxx::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    _client = new BFSS_SNDClient(protocol);
    transport->open();

    EXPECT_TRUE(_client != 0);

}

void BFSS_SN_TestClient::TearDown()
{
    _client->getInputProtocol()->getInputTransport()->close();
    delete _client;

    EXPECT_TRUE(1);
}




/*
  void ManageMessage(MESSAGE_RESULT& _return, const int32_t CmdID, const int32_t Param, const std::string& Data) = 0;

*/
TEST_F(BFSS_SN_TestClient, ManageMessage) {


    MESSAGE_RESULT msg;
    int32_t CmdId = 1;
    int32_t Param = 2;
    std::string data('x', 10);
    _client->ManageMessage(msg, CmdId, Param, data);

    EXPECT_EQ(msg.Result, BFSS_RESULT::type::BFSS_SUCCESS);
}




/*
  BFSS_RESULT::type WriteData(const int32_t index, const int32_t offset, const std::string& data, const std::string& ctx, const int32_t flag) = 0;

*/
TEST_F(BFSS_SN_TestClient, WriteData) {

    int32_t flag = 0;

    auto it = _fid_cache_map.begin();
    for (; it != _fid_cache_map.end(); ++it) {

        std::string fid = it->first;
        object_cache__sn_ &cache = it->second;

        std::string data = fid;
        char c_fill = cache.Size % 26 + 'A';
        data.resize(cache.Size, c_fill);

        BFSS_RESULT::type result = _client->WriteData(cache.BeginIndex, cache.BeginOffset, data, cache.ctx, flag);
        if (result != BFSS_RESULT::type::BFSS_SUCCESS) {
            std::cout << "WriteData Error BeginIndex=>" << cache.BeginIndex << "  BeginOffset=>" << cache.BeginOffset << "  Size=>" << cache.Size << std::endl;
        }
        ASSERT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    }
}


/*
  void CompleteWriteObj(HASH_RESULT& _return, const std::string& ctx) = 0;

*/

TEST_F(BFSS_SN_TestClient, CompleteWriteObj) {

    auto it = _fid_cache_map.begin();
    for (; it != _fid_cache_map.end(); ++it) {

        std::string fid = it->first;
        object_cache__sn_ &cache = it->second;

        HASH_RESULT hash_result;
        _client->CompleteWriteObj(hash_result, cache.ctx);

        EXPECT_EQ(hash_result.Result, BFSS_RESULT::type::BFSS_SUCCESS);
        EXPECT_NE(hash_result.hash.length(), 128);
    }
}


/*
  void ReadData(READ_RESULT& _return, const int32_t index, const int32_t offset, const int32_t size, const int32_t flag, const bool decrypt) = 0;

*/
TEST_F(BFSS_SN_TestClient, ReadData_decrypt) {
    int32_t flag = 1;   /* decrypt */

    std::cout << " ==start of read=====================================decrypt=============" << std::endl;
    auto it = _fid_cache_map.begin();
    for (; it != _fid_cache_map.end(); ++it) {

        std::string fid = it->first;
        object_cache__sn_ &cache = it->second;

        std::string data = fid;
        char c_fill = cache.Size % 26 + 'A';
        data.resize(cache.Size, c_fill);

        READ_RESULT result;
        _client->ReadData(result, cache.BeginIndex, cache.BeginOffset, cache.Size, flag);
        EXPECT_EQ(result.Result, BFSS_RESULT::type::BFSS_SUCCESS);
        if (result.Data != data) {
            std::cout << "ReadData=>" << result.Data.substr(0, 72) << "  len=>" << result.Data.length() << std::endl;
            std::cout << "    Data=>" << fid << "  -SIZE- " << cache.Size << std::endl;
        }
        EXPECT_EQ(result.Data.substr(0, 72), data.substr(0, 72));
        EXPECT_EQ(result.Data.length(), data.length());
        EXPECT_TRUE(result.Data == data);
    }
    std::cout << " ==  end of read=====================================decrypt=============" << std::endl;
}

TEST_F(BFSS_SN_TestClient, ReadData_encrypt) {
    int error_count = 0;
    int32_t flag = 0;   /* Read encrypt data */
    auto it = _fid_cache_map.begin();
    for (; it != _fid_cache_map.end(); ++it) {

        std::string fid = it->first;
        object_cache__sn_ &cache = it->second;

        std::string data = fid;
        char c_fill = cache.Size % 26 + 'A';
        data.resize(cache.Size, c_fill);

        int size = cache.Size;
        size += 16 - ((size-1)%16) -1;   /* 必须16字节对齐 */

        READ_RESULT read;
        _client->ReadData(read, cache.BeginIndex, cache.BeginOffset, size, flag);
        EXPECT_EQ(read.Result, BFSS_RESULT::type::BFSS_SUCCESS);
        EXPECT_NE(read.Data.substr(0, 128), data.substr(0, 128));
        EXPECT_EQ(read.Data.length(), size);

        if (cache.BeginOffset + cache.Size <= blk_size) {

            BLKKEY_RESULT _key;
            _client->GetBlkKey(_key, cache.BeginIndex);
            EXPECT_EQ(_key.Result, BFSS_RESULT::type::BFSS_SUCCESS);

            if (cache.Size >= 16) {
                std::string test = decrypt(_key.Data, read.Data);
                test.resize(cache.Size);
                if (test != data) {
                    std::cout << "ReadData=>" << test.substr(0, 72) << "  len=>" << test.length() << std::endl;
                    std::cout << "    Data=>" << fid << "  -SIZE- " << cache.Size << std::endl;
                    error_count ++;
                }
                EXPECT_TRUE(test == data);
            } else {
                std::string test = decrypt(_key.Data, read.Data);
                test.resize(cache.Size);
                if (test != data) {
                    std::cout << "ReadData=>" << test << "  len=>" << test.length() << std::endl;
                    std::cout << "    Data=>" << fid << "  -SIZE- " << cache.Size << std::endl;
                    error_count ++;
                }
                EXPECT_TRUE(test == data);
            }

        }
    }
    std::cout << "[---BFSS_SN----] Read " << _fid_cache_map.size() << " objects, failed " << error_count << std::endl;

}

TEST_F(BFSS_SN_TestClient, Read_Encrypt_Decrypt) {

    auto it = _fid_cache_map.begin();

    object_cache__sn_ &cache = it->second;
    cache.BeginOffset = (rand() % (blk_size/2)) + blk_size/4;
    cache.BeginOffset += 16 - ((cache.BeginOffset-1)%16) -1;   /* 必须16字节对齐 */
    cache.Size = 64;

    build_cache_context(cache);

    std::string data("-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz");
    data.resize(64);

    BFSS_RESULT::type write_result = _client->WriteData(cache.BeginIndex, cache.BeginOffset, data, cache.ctx, 0);
    EXPECT_EQ(write_result, BFSS_RESULT::type::BFSS_SUCCESS);

    READ_RESULT read_decrypt;
    _client->ReadData(read_decrypt, cache.BeginIndex, cache.BeginOffset, cache.Size, 1/* decrypt */);
    EXPECT_EQ(read_decrypt.Result, BFSS_RESULT::type::BFSS_SUCCESS);
    EXPECT_EQ(read_decrypt.Data, data);

    READ_RESULT read_encrypt;
    _client->ReadData(read_encrypt, cache.BeginIndex, cache.BeginOffset, cache.Size, 0/* encrypt */);
    EXPECT_EQ(read_encrypt.Result, BFSS_RESULT::type::BFSS_SUCCESS);

    BLKKEY_RESULT key;
    _client->GetBlkKey(key, cache.BeginIndex);
    EXPECT_EQ(key.Result, BFSS_RESULT::type::BFSS_SUCCESS);

    EXPECT_EQ(read_decrypt.Data, decrypt(key.Data, (read_encrypt.Data)));

    ///
    std::string enc_data = encrypt(key.Data, data);
    std::string dec_data = decrypt(key.Data, enc_data);
    EXPECT_EQ(dec_data, data);
    std::string dec_sub = decrypt(key.Data, enc_data.substr(16));
    EXPECT_EQ(dec_sub, data.substr(16));
}



TEST_F(BFSS_SN_TestClient, Read_TestAny) {

    /**
     *>  ./BFSS_SN_Test --gtest_filter=BFSS_SN_TestClient.Read_TestAny
     */

    return;

    object_cache__sn_ cache;
    cache.BeginIndex = 9;
    cache.BeginOffset = 534000;      // 16字节对齐
    cache.Size = 16;

    READ_RESULT read_decrypt;
    _client->ReadData(read_decrypt, cache.BeginIndex, cache.BeginOffset, cache.Size, 1/* decrypt */);
    EXPECT_EQ(read_decrypt.Result, BFSS_RESULT::type::BFSS_SUCCESS);
    EXPECT_EQ(read_decrypt.Data, "QtI3tQ8IvdLhGKEx");
}



#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <unistd.h>

TEST(BFSS_SN_R, Reset_SN_blkdevice) {

    /**
     *>  ./BFSS_SN_Test --gtest_filter=BFSS_SN_R.Reset_SN_blkdevice
     */
    char *psz_dev = getenv("GTEST_BFSS_RESET_SN");/* limit the  for     max size ->  1024 */
    if (!psz_dev) {
        std::cout << "### export GTEST_BFSS_RESET_SN=/dev/nvme1n1" << std::endl;
        std::cout << "### ./BFSS_SN_Test --gtest_filter=BFSS_SN_R.Reset_SN_blkdevice" << std::endl;
        EXPECT_TRUE(psz_dev);
        return;
    }

    std::cout << "export GTEST_BFSS_RESET_SN=" << psz_dev << std::endl;
    int _fd = ::open(psz_dev, O_RDWR); /*eg. "/dev/nvme1n1"     */
    EXPECT_FALSE(_fd == -1);

    const int _mask_len = 8;/*
    const unsigned char _mask[] = {'B', 'F', 'S', 'S', '-', 'S', 'N', '\0'};*/
    const unsigned char _mask[] = "RESET-SN";

    EXPECT_TRUE(_mask_len == pwrite64(_fd, _mask, _mask_len, 0));
}

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
TEST(BFSS_SN_R, Encrypt_Decrypt) {
    /**
     *>  ./BFSS_SN_Test --gtest_filter=BFSS_SN_R.Encrypt_Decrypt
     *>  time dd if=/dev/nvme1n1 bs=5M count=1024 of=/dev/null
     *>  time dd if=/dev/zero of=/dev/nvme1n1 bs=5M count=1024
     *>  dstat -cdnm --tcp
     *>  htop
     */
    unsigned char key[EVP_MAX_KEY_LENGTH * 2] = {0};
    unsigned char iv[EVP_MAX_IV_LENGTH * 2] = {0};

    {
        int key_len = EVP_CIPHER_key_length(EVP_aes_256_ecb());
        int iv_len = EVP_CIPHER_iv_length(EVP_aes_256_ecb());
        EXPECT_EQ(key_len, 32);
        EXPECT_EQ(iv_len, 0);

        key_len = EVP_CIPHER_key_length(EVP_aes_256_cbc());
        iv_len = EVP_CIPHER_iv_length(EVP_aes_256_cbc());
        EXPECT_EQ(key_len, 32);
        EXPECT_EQ(iv_len, 16);
    }

    const unsigned char test_data[128] = "-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz-.0123456789ABCDEFGHIMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
    auto test_data_len = std::strlen((char*)test_data);
    unsigned char encrypt_out[256] = {0};
    unsigned char decrypt_out[256] = {0};
    int encrypt_len = 0;
    int decrypt_len = 0;
    string tmp;

    std::map<string, const EVP_CIPHER *> cipher_map_;
    cipher_map_["EVP_aes_128_ecb"] = EVP_aes_128_ecb();
    cipher_map_["EVP_aes_128_cbc"] = EVP_aes_128_cbc();
    cipher_map_["EVP_aes_128_cfb"] = EVP_aes_128_cfb();
    cipher_map_["EVP_aes_256_ecb"] = EVP_aes_256_ecb();
    cipher_map_["EVP_aes_256_cbc"] = EVP_aes_256_cbc();
    cipher_map_["EVP_aes_256_cfb"] = EVP_aes_256_cfb();
    for (auto it : cipher_map_) {

        // 不同长度，
        int _len=rand()%(test_data_len/4);
        while ( _len < test_data_len) {
            _len += 3;
            if (_len > test_data_len) {
                _len = test_data_len;
            }

            cout << std::endl << " ==" << _len << " ===================== " << it.first
                 << " ========================== "
                 << std::endl;
            const EVP_CIPHER *cipher = it.second;

            unsigned char salt[32] = {0};
            int salt_result = RAND_bytes(salt, 32);
            EXPECT_EQ(salt_result, 1);

            unsigned char data[8] = {0};
            ((int32_t *) data)[0] = 10022;
            ((int32_t *) data)[1] = 123;
            int key_len = EVP_BytesToKey(cipher, EVP_sha1(), salt, data, sizeof(data), 32, key, iv);
            EXPECT_EQ(key_len, EVP_CIPHER_key_length(cipher));
            /************************************************************/

            EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
            int ret = EVP_EncryptInit_ex(ctx, cipher, nullptr, key, iv);
            EXPECT_TRUE(ret == 1);

            ret = EVP_EncryptUpdate(ctx, encrypt_out, &encrypt_len, test_data, _len);
            cout << bin2hex(string((char *) encrypt_out, encrypt_len), tmp) << "   encrypt_len=" << encrypt_len
                 << std::endl;
            EXPECT_TRUE(ret == 1);

            int tail_len = 0;
            ret = EVP_EncryptFinal_ex(ctx, encrypt_out + encrypt_len, &tail_len);
            cout << bin2hex(string((char *) encrypt_out, encrypt_len + tail_len), tmp) << "   final encrypt_len="
                 << encrypt_len + tail_len << std::endl;
            EXPECT_TRUE(ret == 1);
            /************************************************************/

            ret = EVP_DecryptInit_ex(ctx, cipher, nullptr, key, iv);
            EXPECT_TRUE(ret == 1);

            ret = EVP_DecryptUpdate(ctx, decrypt_out, &decrypt_len, encrypt_out, encrypt_len + tail_len);
            cout << string((char *) decrypt_out, decrypt_len) << "   decrypt_out=" << decrypt_len << std::endl;
            EXPECT_TRUE(ret == 1);

            ret = EVP_DecryptFinal_ex(ctx, decrypt_out + decrypt_len, &tail_len);
            EXPECT_TRUE(ret == 1);
            cout << string((char *) decrypt_out, decrypt_len + tail_len) << "   final decrypt_out="
                 << decrypt_len + tail_len << std::endl;
            EXPECT_TRUE(ret == 1);
            EXPECT_EQ(decrypt_len + tail_len, _len);
            EXPECT_EQ(string((char *) decrypt_out, _len), string((char *) test_data, _len));

            _len += rand() % (test_data_len / 4);
        }
    }
}