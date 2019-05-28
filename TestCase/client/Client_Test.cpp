

#include "Client_Test.h"
#include "bfss_config.h"
#include <algorithm>
#include <tuple>
#include <bfss_utils.h>
#include <bfss_result_types.h>

using namespace bfss;


/* 预设 不同大小的 文件fid  */
static std::map<std::string, int32_t> _fid_size_map;
static std::map<std::string, std::string> _fid_hash_map;
static std::string _tag("TAG=BFSS_API_T");
static int rw_chip_size = 1<<15;    /*15=>32K*/

void BFSS_API_TestBase::SetUpTestCase()
{
    srand(time(nullptr)+rand());
    _fid_hash_map.clear();
    _fid_size_map.clear();
    /* 环境变量控制运行时参数
export GTEST_BFSS_Z=26
export GTEST_BFSS_Y=125         # oid 长度
export GTEST_BFSS_X=16
export GTEST_BFSS_S=400
export GTEST_BFSS_R=1024
export GTEST_BFSS_T=BFSS_API_T
     ================== */
    char *pszZ = getenv("GTEST_BFSS_Z");/* limit the   file's max size ->  2^z */
    char *pszX = getenv("GTEST_BFSS_X");/* limit the random's max size ->  2^x */
    char *pszS = getenv("GTEST_BFSS_S");/* limit the  total's max size ->  S MBytes */
    char *pszR = getenv("GTEST_BFSS_R");/* limit the   file's count    ->  1024 */
    char *pszT = getenv("GTEST_BFSS_T");if (pszT) _tag = pszT;/*定制标签，区分gtest客户端*/
    char *pszC = getenv("GTEST_BFSS_C");/* read/write   chip size      ->  2^c */

    // 初始化文件信息   只确定文件大小 文件内容根据大小临时构造
    int z = 24;  if (pszZ) z = atoi(pszZ);/* limit the   file's max size ->  2^z  24=>16M*/
    int x = z-4; if (pszX) x = atoi(pszX);/* limit the random's max size ->  2^x  20=>768K~1M*/
    int s = 300; if (pszS) s = atoi(pszS);/* limit the  total's max size ->  s    300 MBytes*/
    int r = 1024;if (pszR) r = atoi(pszR);/* limit the   file's count    ->  r    1024个文件*/
    int c = 20;  if (pszC) c = atoi(pszC);/* read/write   chip size      ->  2^c  20=>1M*/
    rw_chip_size = (1<<c);

    std::cout << "export GTEST_BFSS_Z=" << z << std::endl;
    std::cout << "export GTEST_BFSS_X=" << x << std::endl;
    std::cout << "export GTEST_BFSS_S=" << s << std::endl;
    std::cout << "export GTEST_BFSS_R=" << r << std::endl;
    std::cout << "export GTEST_BFSS_C=" << c << std::endl;
    std::cout << "export GTEST_BFSS_T=" << _tag << std::endl;

    int32_t size = 0;
    int64_t offset = 0;
    for (int i=0; i<r; i++) {
        if (i < z) {
            size = 1 << i;         // 指数递增的文件大小
        } else {
            size = (rand() % (1 << (x - 2))) + (1 << (x - 2)) + (1 << (x - 1)); // rand(1/4) + fix(3/4)
        }
        offset += size;
        if (offset > ((int64_t) s) * ((int64_t) _1M_SIZE)) {
            break;
        }

        std::string fid = gen_oid();    // gen_fid();
        _fid_size_map[fid] = size;

        std::cout << std::setw(4) << i << ".   fid=[" << fid << "]   size=" << size << std::endl;
    }

    EXPECT_TRUE(_fid_size_map.size() > 0);
    std::cout << "--setup -" <<  _fid_size_map.size()  << "- file objects--." << std::endl;
}

void BFSS_API_TestBase::TearDownTestCase()
{

    // 停止服务
}

void BFSS_API_TestBase::SetUp()
{
/* 环境变量控制运行时参数
export GTEST_BFSS_API_IP=127.0.0.1
export GTEST_BFSS_API_PORT=9092
 */
    char *pszIP   = getenv("GTEST_BFSS_API_IP");
    char *pszPORT = getenv("GTEST_BFSS_API_PORT");
    std::string ip("127.0.0.1");    if (pszIP)      ip = std::string(pszIP);
    int port = 9092;                if (pszPORT)  port = atoi(pszPORT);
    static bool test_output = false;
    if (!test_output) {
        std::cout << "export GTEST_BFSS_API_IP="   <<   ip << std::endl;
        std::cout << "export GTEST_BFSS_API_PORT=" << port << std::endl;
        test_output = true;
    }

    stdcxx::shared_ptr<TTransport> socket(new TSocket(ip.c_str(), port));
    stdcxx::shared_ptr<TTransport> transport(new TFramedTransport(socket));
    stdcxx::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    _client = new BFSS_APIDClient(protocol);
    transport->open();

    EXPECT_TRUE(_client != 0);

}

void BFSS_API_TestBase::TearDown()
{
    _client->getInputProtocol()->getInputTransport()->close();
    delete _client;

    EXPECT_TRUE(1);
}


/*
  BFSS_RESULT::type CreateObject(const std::string& fid, const int32_t size, const int32_t flag, const std::string& tag);
*/
TEST_F(BFSS_API_TestBase, CreateObject) {

    int32_t     flag = 0;

    auto it = _fid_size_map.begin();
    for ( ; it != _fid_size_map.end(); ++it) {

        const std::string& fid = it->first;
        int32_t size = it->second;

        BFSS_RESULT::type result = _client->CreateObject(fid, size, flag, _tag);
        EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    }

    // 重复创建
    BFSS_RESULT::type result = _client->CreateObject(_fid_size_map.begin()->first, _fid_size_map.begin()->second, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_UNKNOWN_ERROR/*BFSS_DUPLICATED*/);
}



/*
  BFSS_RESULT::type Write(const std::string& fid, const int32_t offset, const std::string& data);
*/
TEST_F(BFSS_API_TestBase, Write) {

    auto it = _fid_size_map.begin();
    for (; it != _fid_size_map.end(); ++it) {

        const std::string &fid = it->first;
        int32_t size = it->second;

        std::string data = fid;
        char c_fill = size % 26 + 'A';
        data.resize(size, c_fill);

        SHA256_CTX  sha256_ctx;
        SHA256_Init(&sha256_ctx);
        unsigned char tmp[SHA256_DIGEST_LENGTH];

        int32_t write_size = std::min(7, size);
        std::string write_data = data.substr(0, write_size);
        BFSS_RESULT::type result = _client->Write(fid, 0, write_data);
        SHA256_Update(&sha256_ctx, write_data.c_str(), write_size);
        EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

        int32_t offset = write_size;
        while (offset < size) {
            write_size = std::min(size - offset, rw_chip_size);
            write_data = data.substr(offset, write_size);
            result = _client->Write(fid, offset, write_data);
            SHA256_Update(&sha256_ctx, write_data.c_str(), write_size);
            EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
            offset += write_size;
        }
        SHA256_Final(tmp, &sha256_ctx);
        std::string &hex = _fid_hash_map[fid];
        hex.resize(SHA256_DIGEST_LENGTH * 2);
        bfss::bin2hex(std::string((const char *) tmp, SHA256_DIGEST_LENGTH),hex);
    }
}


/*
  BFSS_RESULT::type WriteComplete(const std::string& fid);
*/
TEST_F(BFSS_API_TestBase, CompleteObject) {


    auto it = _fid_size_map.begin();
    for ( ; it != _fid_size_map.end(); ++it) {

        const std::string& fid = it->first;

        BFSS_RESULT::type result = _client->CompleteObject(fid);
        EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    }

    /* 重复API调用 CompleteObject  */
    it = _fid_size_map.begin();
    for ( ; it != _fid_size_map.end(); ++it) {

        const std::string& fid = it->first;

        BFSS_RESULT::type result = _client->CompleteObject(fid);
        EXPECT_EQ(result, BFSS_RESULT::type::BFSS_COMPLETED);   //NOT   BFSS_RESULT::type::BFSS_DUP_ID
    }
}



/*
  void GetFileInfo(FILE_INFO_RESULT& _return, const std::string& fid);
*/

TEST_F(BFSS_API_TestBase, GetFileInfo) {

    auto it = _fid_size_map.begin();
    for (; it != _fid_size_map.end(); ++it) {

        const std::string& fid = it->first;
        int32_t size = it->second;

        OBJECT_INFO_RESULT finfo;
        _client->GetObjectInfo(finfo, fid);

        EXPECT_EQ(finfo.Result, BFSS_RESULT::type::BFSS_SUCCESS);
        EXPECT_EQ(finfo.ObjectInfo.ObjectSize, size);
        EXPECT_EQ(finfo.ObjectInfo.ObjectTag, _tag);
        EXPECT_EQ(finfo.ObjectInfo.Hash, _fid_hash_map[fid]);
    }
}

/*
  void ObjectLockHasHash(OBJECT_HASHASH_INFO_RESULT& _return, const std::string& hash, const int32_t size, const std::string& head);
*/
TEST_F(BFSS_API_TestBase, ObjectLockHasHash) {


    auto it = _fid_size_map.begin();
    for ( ; it != _fid_size_map.end(); ++it) {

        const std::string& fid = it->first;
        int32_t size = it->second;

        string& hash = _fid_hash_map[fid];
        string head = fid.substr(0, std::min(16, size));

        BFSS_RESULT::type hresult = _client->ObjectLockHasHash(hash, size, head);
        if (hresult != BFSS_RESULT::type::BFSS_SUCCESS) {
            std::cout << fid << " -- hash --> " << hash << "   -size->" << size << std::endl;
        }
        EXPECT_EQ(hresult,  BFSS_RESULT::type::BFSS_SUCCESS);
    }
}


/*
  void CreateObjectLink(const std::string& oid, const std::string& hash, const int32_t size, const std::string& head,
                        const int32_t flag, const std::string& tag);
*/
TEST_F(BFSS_API_TestBase, CreateObjectLink) {
    auto old_fid_size_map = _fid_size_map;
    auto it = old_fid_size_map.begin();
    for (int i=0 ; it != old_fid_size_map.end() && i<16; ++it, i++) {

        const std::string& fid = it->first;
        int32_t size = it->second;
        if (size < fid.length()*2) {    /*确保去掉fid后还有数据，方便比较*/
            continue;
        }

        std::string link_oid = "Ztest-CreateObjectLink--" + gen_oid().substr(32);

        string& hash = _fid_hash_map[fid];
        string head = fid.substr(0, std::min(16, size));

        BFSS_RESULT::type hresult = _client->CreateObjectLink(link_oid, hash, size, head, 0, _tag);
        if (hresult != BFSS_RESULT::type::BFSS_SUCCESS) {
            std::cout << "CreateObjectLink Faild:" << link_oid << " -- hash --> " << hash << "   -size->" << size << std::endl;
        }
        EXPECT_EQ(hresult,  BFSS_RESULT::type::BFSS_SUCCESS);
        _fid_size_map[link_oid] = size;
    }

    it = old_fid_size_map.begin();
    for (int i=0 ; it != old_fid_size_map.end() && i<32; ++it, i++) {

        const std::string& fid = it->first;
        int32_t size = it->second;
        if (size < fid.length()*2) {    /*确保去掉fid后还有数据，方便比较*/
            continue;
        }

        std::string link_oid = "Ztest-CreateObjectLink--" + gen_oid().substr(24);

        string& hash = _fid_hash_map[fid];
        string head = fid.substr(0, std::min(16, size));

        BFSS_RESULT::type hresult = _client->CreateObjectLink(link_oid, hash, size, head, 0, "test-CreateObjectLink");
        if (hresult != BFSS_RESULT::type::BFSS_SUCCESS) {
            std::cout << "CreateObjectLink Faild:" << link_oid << " -- hash --> " << hash << "   -size->" << size << std::endl;
        }
        EXPECT_EQ(hresult,  BFSS_RESULT::type::BFSS_SUCCESS);
        _fid_size_map[link_oid] = size;
    }
    old_fid_size_map.clear();
}


/*
  void ResizeObject(const std::string& oid, const int32_t newsize);
*/
TEST_F(BFSS_API_TestBase, ResizeObject) {
    /*>>> ./BFSS_API_Test --gtest_filter=BFSS_API_TestBase.ResizeObject    */
    string fid = "Ztest-ResizeObject--" + gen_oid().substr(32);
    int32_t size = int32_t(rand() % (_1M_SIZE/2)) + int32_t(_1M_SIZE/4) + (int32_t)fid.length();

    BFSS_RESULT::type result = _client->CreateObject(fid, size, 0, "test-resize");
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    int new_size = rand() % (size/2) + (size/4);
    result = _client->ResizeObject(fid, new_size);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    std::string data = fid;
    char c_fill = size % 26 + 'A';
    data.resize(size, c_fill);
    result = _client->Write(fid, 0, data.substr(0, new_size));
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    int final_size = rand() % (new_size/2) + (new_size/4);
    result = _client->ResizeObject(fid, final_size);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    std::string final_data = fid;
    char final_fill = final_size % 26 + 'A';
    final_data.resize(final_size, final_fill);
    result = _client->Write(fid, 0, final_data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->ResizeObject(fid, new_size);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_PARAM_ERROR);

    result = _client->CompleteObject(fid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    _fid_size_map[fid] = final_size;
    std::cout << "ResizeObject fid=" << fid << "  from " << size << " to " << final_size << std::endl;

    result = _client->ResizeObject(fid, new_size);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_COMPLETED);
}

/*
  void Read(READ_RESULT& _return, const std::string& fid, const int32_t size, const int32_t offset);
*/
TEST_F(BFSS_API_TestBase, Read) {


    auto it = _fid_size_map.begin();
    for ( ; it != _fid_size_map.end(); ++it) {

        const std::string& fid = it->first;
        const int32_t size = it->second;

        std::string read_data;
        int32_t offset = 0;
        while (offset < size) {
            READ_RESULT read;

            int32_t read_size = std::min(size - offset, rw_chip_size);
            _client->Read(read, fid, read_size, offset);
            EXPECT_EQ(read.Result, BFSS_RESULT::type::BFSS_SUCCESS);
            offset += read_size;

            read_data += read.Data;
        }

        std::string data = fid;
        char c_fill = size % 26 + 'A';
        data.resize(size, c_fill);

        if (fid.find("Ztest-CreateObjectLink-") == 0 && read_data.substr(fid.length()+32) == data.substr(fid.length()+32)) {
            continue;
        }

        if (read_data != data) {
            std::cout << "ReadData=>" << read_data.substr(0, fid.length()) << "  len=>" << read_data.length() << std::endl;
            std::cout << "    Data=>" << fid << "  -SIZE- " << size << std::endl;

            const char* pwsRead = read_data.c_str();
            const char* pwsWrite = data.c_str();
            int i = 0;
            while ( pwsRead[i] == pwsWrite[i]) {++i;}
            std::cout << " Diff at=>" << i << "  -diff- " << read_data.substr(i, i+256) << std::endl;
        }
        EXPECT_TRUE(read_data == data);
        EXPECT_EQ(read_data.length(), data.length());
    }
}



/*
  void ReadBlk(READ_RESULT& _return, const std::string& fid, const int32_t size, const int32_t offset);

*/
TEST_F(BFSS_API_TestBase, ReadBlk) {
    int error_count = 0;
    int32_t offset = 0;

    auto it = _fid_size_map.begin();
    for (; it != _fid_size_map.end(); ++it) {

        const std::string& fid = it->first;
        const int32_t size = it->second;

        std::string data = fid;
        char c_fill = size % 26 + 'A';
        data.resize(size, c_fill);


        std::string read_data;
        int32_t read_size = size + 16 - ((size-1)%16) -1;   /* 必须16字节对齐 */
        int32_t offset = 0;
        while (read_size > 0) {
            READ_RESULT read;

            int32_t  _size = std::min(_1M_SIZE, read_size);
            _client->ReadBlk(read, fid, _size, offset);
            EXPECT_EQ(read.Result,  BFSS_RESULT::type::BFSS_SUCCESS);
            read_size -= _1M_SIZE;
            offset += _1M_SIZE;

            read_data += read.Data;
        }

        //  get key and  decrypt ...
        OBJECT_BLKS_RESULT blksResult;
        _client->GetObjectBlksInfo(blksResult, fid);
        EXPECT_EQ(blksResult.Result, BFSS_RESULT::type::BFSS_SUCCESS);

        std::string _decrypt_result;
        auto it = blksResult.ObjectBlksInfo.Blks.begin();
        for (; it != blksResult.ObjectBlksInfo.Blks.end(); ++it) {
            const OBJECT_BLK_INFO& blk = *it;

            BLK_KEY_RESULT _key;
            _client->GetObjectBlkKey(_key, fid, blk.DataOff);
            EXPECT_EQ(_key.Result, BFSS_RESULT::type::BFSS_SUCCESS);
            EXPECT_EQ(_key.BlkKey, blk.BlkKey);
            EXPECT_TRUE(blk.DataOff < read_data.size());

            std::string _blk_decrypt = decrypt(_key.BlkKey, read_data.substr(blk.DataOff, blk.BlkSize));
            _decrypt_result += _blk_decrypt;
        }
        _decrypt_result.resize(size);

        if (fid.find("Ztest-CreateObjectLink-") == 0 && _decrypt_result.substr(fid.length()+32) == data.substr(fid.length()+32)) {
            continue;
        }
        if (_decrypt_result != data) {
            std::cout << "ReadData=>" << _decrypt_result.substr(0, 72) << "  len=>" << _decrypt_result.length() << std::endl;
            std::cout << "    Data=>" << fid << "  -SIZE- " << size << std::endl;

            const char* pwsRead = _decrypt_result.c_str();
            const char* pwsWrite = data.c_str();
            int i = 0;
            while ( pwsRead[i] == pwsWrite[i]) {++i;}
            std::cout << " Diff at=>" << i << "  -diff- " << _decrypt_result.substr(i, i+256) << std::endl;

            error_count ++;
        }
        EXPECT_TRUE(_decrypt_result == data);
    }
    std::cout << "[---BFSS_API---] Read " << _fid_size_map.size() << " objects, failed " << error_count << std::endl;
}

/*
  void GetObjectBlksInfo(FILE_BLKS_RESULT& _return, const std::string& fid);

*/
TEST_F(BFSS_API_TestBase, GetObjectBlksInfo) {

    int32_t offset = 0;

    auto it = _fid_size_map.begin();
    for ( ; it != _fid_size_map.end(); ++it) {

        const std::string& fid = it->first;
        int32_t size = it->second;

        OBJECT_BLKS_RESULT blksResult;
        _client->GetObjectBlksInfo(blksResult, fid);

        EXPECT_EQ(blksResult.Result ,  BFSS_RESULT::type::BFSS_SUCCESS);
        if (blksResult.ObjectBlksInfo.ObjectInfo.ObjectSize != size) {
            std::cout << "GetObjectBlksInfo fid=" << fid << std::endl;
        }
        EXPECT_EQ(blksResult.ObjectBlksInfo.ObjectInfo.ObjectSize, size);
        EXPECT_TRUE(blksResult.ObjectBlksInfo.Blks.size() > 0);
        if (blksResult.ObjectBlksInfo.Blks.size()) {
            EXPECT_TRUE(blksResult.ObjectBlksInfo.Blks[0].BlkKey.size() > 0);
        }
    }
}


/*
  void GetObjectBlkKey(BLK_KEY_RESULT& _return, const std::string& fid, const int32_t offset);

*/
TEST_F(BFSS_API_TestBase, GetObjectBlkKey) {


    int32_t offset = 0;

    auto it = _fid_size_map.begin();
    for ( ; it != _fid_size_map.end(); ++it) {

        const std::string& fid = it->first;
        int32_t size = it->second;

        BLK_KEY_RESULT blkkey;
        _client->GetObjectBlkKey(blkkey, fid, offset);

        EXPECT_EQ(blkkey.Result,  BFSS_RESULT::type::BFSS_SUCCESS);
        EXPECT_TRUE(blkkey.BlkKey.length() > 0);
    }
}


/*
  void ManageMessage(MESSAGE_RESULT& _return, const int32_t CmdId, const int32_t Param, const std::string& Data);
  */
TEST_F(BFSS_API_TestBase, ManageMessage) {

    MESSAGE_RESULT msg;
    int32_t CmdId = 1;
    int32_t Param = 2;
    std::string data(" BFSS_API_TestBase ManageMessage -");
    _client->ManageMessage(msg, CmdId, Param, data);

    EXPECT_EQ(msg.Result, BFSS_RESULT::type::BFSS_SUCCESS);
}



/*
  BFSS_RESULT::type DeleteObject(const std::string& fid);

*/
TEST_F(BFSS_API_TestBase, DeleteObject) {

    auto it = _fid_size_map.begin();
    for (; it != _fid_size_map.end(); ++it) {
        const std::string& fid = it->first;

        BFSS_RESULT::type result = _client->DeleteObject(fid);
        EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    }

    // 重复删除
    BFSS_RESULT::type result = _client->DeleteObject(_fid_size_map.begin()->first);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_NOTFOUND);
}

TEST_F(BFSS_API_Test_Exceptions, CreateObject_for_InvalidOID) {
    int32_t flag = 1;
    //std::string fid = "`fdafa&fsdafda!dafdas_1231";
    std::string oid = gen_oid() + "!";
    int32_t size = 1*1024*1024*1024 + 500*1024*1024 + 1;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_PARAM_ERROR);

}
TEST_F(BFSS_API_Test_Exceptions, CreateObject_for_Param) {
    int32_t flag = 1;
    //std::string fid = ".";
    std::string oid = gen_oid();

    int32_t size =  65;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

}
TEST_F(BFSS_API_Test_Exceptions, CreateObject_for_nullOID) {
    int32_t flag = 1;
    std::string fid = "";
    int32_t  size =  30;
    BFSS_RESULT::type result = _client->CreateObject(fid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_PARAM_ERROR);
}

TEST_F(BFSS_API_Test_Exceptions, DeleteObject_for_nullOID) {
    std::string fid = "";
    BFSS_RESULT::type result = _client->DeleteObject(fid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_PARAM_ERROR);
}

TEST_F(BFSS_API_Test_Exceptions, Write_for_offset_over) {

    int32_t flag = 1;
    //std::string oid = "1.";
    std::string oid = gen_oid();
    int32_t size =  65;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset = 67;
    std::string data = "13212313";

    result = _client->Write(oid, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_PARAM_ERROR);

    result = _client->Write(oid, 5, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

//    sleep(65);
    data="098765";
    result = _client->Write(oid, 0, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->CompleteObject(oid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
}

TEST_F(BFSS_API_Test_Exceptions, Write_for_data_over) {
    int32_t flag = 1;
    //std::string oid = "2.";
    std::string oid = gen_oid();
    int32_t size =  65;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset = 60;
    const std::string data = "0123456789";
    result = _client->Write(oid, offset, data);
    //hash _hash_map NOT found == !( 0 and begin)
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_DATA_WRITE_INCOMPLETE);

    result = _client->CompleteObject(oid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
}

TEST_F(BFSS_API_Test_Exceptions, Write_for_data_test_encypt_aglin) {
    int32_t flag = 1;
    //std::string oid = "3.";
    std::string oid = gen_oid();
    int32_t size =  65;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset = 0;
    const std::string data = "0123456789abcdef";
    result = _client->Write(oid, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset1 = 18;
    const std::string data1 = "0123456789abcdef";
    result = _client->Write(oid, offset1, data1);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset2 = 48;
    const std::string data2 = "0123456789abcdef";
    result = _client->Write(oid, offset2, data2);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->CompleteObject(oid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
}

TEST_F(BFSS_API_Test_Exceptions, Write_for_data_test_encypt) {
    int32_t flag = 1;
    //std::string oid = "4.";
    std::string oid = gen_oid();
    int32_t size =  65;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset = 0;
    const std::string data = "0123456789ab";
    result = _client->Write(oid, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset1 = 18;
    const std::string data1 = "0123456789ab";
    result = _client->Write(oid, offset1, data1);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset2 = 48;
    const std::string data2 = "0123456789ab";
    result = _client->Write(oid, offset2, data2);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->CompleteObject(oid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
}

TEST_F(BFSS_API_Test_Exceptions, Write_for_data_ok) {
    int32_t flag = 1;
    //std::string oid = "5.";
    std::string oid = gen_oid();
    int32_t size =  65;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset = 0;
    const std::string data = "0123456789abcdef";
    result = _client->Write(oid, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset1 = 16;
    const std::string data1 = "0123456789abcdef0123456789abcdef";
    result = _client->Write(oid, offset1, data1);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset2 = 48;
    const std::string data2 = "0123456789abcdef";
    result = _client->Write(oid, offset2, data2);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->CompleteObject(oid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
}

TEST_F(BFSS_API_Test_Exceptions, Write_for_data_back) {
    int32_t flag = 1;
    //std::string oid = "6.";
    std::string oid = gen_oid();
    int32_t size =  65;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset = 48;
    const std::string data = "0123456789abcdef";
    result = _client->Write(oid, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset1 = 16;
    const std::string data1 = "0123456789abcdef0123456789abcdef";
    result = _client->Write(oid, offset1, data1);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset2 = 0;
    const std::string data2 = "0123456789abcdef";
    result = _client->Write(oid, offset2, data2);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->CompleteObject(oid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
}

TEST_F(BFSS_API_Test_Exceptions, Write_for_data_other) {
    int32_t flag = 1;
    //std::string oid = "7.";
    std::string oid = gen_oid();
    int32_t size =  65;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset = 0;
    const std::string data = "0123456789abcdef";
    result = _client->Write(oid, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset1 = 16;
    const std::string data1 = "0123456789abcdef0123456789abcdef0123";
    result = _client->Write(oid, offset1, data1);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset2 = 52;
    const std::string data2 = "456789abcdef";
    result = _client->Write(oid, offset2, data2);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->CompleteObject(oid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
}

TEST_F(BFSS_API_Test_Exceptions, Write_for_data_repeat) {
    int32_t flag = 1;
    //std::string oid = "6.";
    //std::string oid1 = "7.";
    std::string oid = gen_oid();
    std::string oid1 = gen_oid();
    int32_t size =  32;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->CreateObject(oid1, size, flag, _tag);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    int32_t offset = 0;
    std::string data = "0123456789abcdef";
    result = _client->Write(oid, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->Write(oid1, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    offset = 16;
    data = "0123456789abcdef";
    result = _client->Write(oid, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->Write(oid1, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->CompleteObject(oid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->CompleteObject(oid1);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

}


TEST_F(BFSS_API_Test_Exceptions, Read_for_OK) {
    std::string oid = gen_oid();
    int32_t flag = 1;
    int32_t size = 31;
    BFSS_RESULT::type result = _client->CreateObject(oid, size, flag, "Read_For_OK");
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset = 0;
    const std::string data = "0123456789abcdef";
    result = _client->Write(oid, offset, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    const int32_t offset1 = 16;
    const std::string data1 = "0123456789abcde";
    result = _client->Write(oid, offset1, data1);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->CompleteObject(oid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    READ_RESULT r;
    _client->Read(r, oid, 15, 0);
    EXPECT_EQ(r.Result, BFSS_RESULT::type::BFSS_SUCCESS);
    _client->Read(r, oid, 31, 15);
    EXPECT_EQ(r.Result, BFSS_RESULT::type::BFSS_DATA_READ_INCOMPLETE);
    _client->Read(r, oid, 45, 15);
    EXPECT_EQ(r.Result, BFSS_RESULT::type::BFSS_DATA_READ_INCOMPLETE);
    _client->Read(r, oid, 45, 45);
    EXPECT_EQ(r.Result, BFSS_RESULT::type::BFSS_PARAM_ERROR);
}

TEST_F(BFSS_API_Test_Exceptions, Write_deduplication) {

    string fid1 = gen_oid();
    string fid2 = gen_oid();

    int32_t size = int32_t(rand() % (_1M_SIZE/2)) + (_1M_SIZE/4) + (int32_t)fid1.length();
    std::string data = fid1;
    char c_fill = size % 26 + 'A';
    data.resize(size, c_fill);

    BFSS_RESULT::type result = _client->CreateObject(fid1, size, 0, "rest-for-deduplication-1");
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    /*BFSS_RESULT::type*/ result = _client->CreateObject(fid2, size, 0, "rest-for-deduplication-2");
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->Write(fid1, 0, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->Write(fid2, 0, data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);


    result = _client->CompleteObject(fid1);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->CompleteObject(fid2);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    OBJECT_BLKS_RESULT blk1_result, blk2_result;
    _client->GetObjectBlksInfo(blk1_result, fid1);
    _client->GetObjectBlksInfo(blk2_result, fid2);
    EXPECT_EQ(blk1_result.Result, BFSS_RESULT::type::BFSS_SUCCESS);
    EXPECT_EQ(blk2_result.Result, BFSS_RESULT::type::BFSS_SUCCESS);

    EXPECT_EQ(blk1_result.ObjectBlksInfo.ObjectInfo.ObjectSize, blk2_result.ObjectBlksInfo.ObjectInfo.ObjectSize);
    EXPECT_EQ(blk1_result.ObjectBlksInfo.Blks,       blk2_result.ObjectBlksInfo.Blks);
    EXPECT_NE(blk1_result.ObjectBlksInfo.ObjectInfo.CreateTime, blk2_result.ObjectBlksInfo.ObjectInfo.CreateTime);

    OBJECT_INFO_RESULT obj1_result, obj2_result;
    _client->GetObjectInfo(obj1_result, fid1);
    _client->GetObjectInfo(obj2_result, fid2);
    EXPECT_EQ(obj1_result.Result, BFSS_RESULT::type::BFSS_SUCCESS);
    EXPECT_EQ(obj2_result.Result, BFSS_RESULT::type::BFSS_SUCCESS);
    EXPECT_EQ(obj1_result.ObjectInfo.ObjectSize, obj2_result.ObjectInfo.ObjectSize);
    EXPECT_EQ(obj1_result.ObjectInfo.Hash,       obj2_result.ObjectInfo.Hash);
    EXPECT_EQ(obj1_result.ObjectInfo.Complete,   obj2_result.ObjectInfo.Complete);
    EXPECT_NE(obj1_result.ObjectInfo.ObjectTag,  obj2_result.ObjectInfo.ObjectTag);

    BFSS_RESULT::type  del_result = _client->DeleteObject(fid1);
    EXPECT_EQ(del_result, BFSS_RESULT::type::BFSS_SUCCESS);
    del_result = _client->DeleteObject(fid2);
    EXPECT_EQ(del_result, BFSS_RESULT::type::BFSS_SUCCESS);
    del_result = _client->DeleteObject(fid2);
    EXPECT_EQ(del_result, BFSS_RESULT::type::BFSS_NOTFOUND);
}


TEST_F(BFSS_API_Test_Exceptions, Write_Disordered) {
    /*>>> ./BFSS_API_Test --gtest_filter=BFSS_API_Test_Exceptions.Write_Disordered    */
    string fid = gen_oid();
    int32_t size = int32_t(rand() % (_1M_SIZE/2)) + (_1M_SIZE/4) + (int32_t)fid.length();

    BFSS_RESULT::type result = _client->CreateObject(fid, size, 0, "test-disordered");
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    /*先写入fid*/
    std::string begin_data = fid;
    result = _client->Write(fid, 0, begin_data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    /*然后写入最后一段*/
    int32_t end_size = rand()%(_1M_SIZE/8) + (_1M_SIZE/8);
    string end_data(end_size, 'Z');
    result = _client->Write(fid, size - end_size, end_data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    /*最后写入中间一段*/
    int32_t mid_size = (int32_t)(size - end_size - fid.length());
    string mid_data(mid_size, 'M');
    result = _client->Write(fid, (int32_t)begin_data.length(), mid_data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->CompleteObject(fid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->CompleteObject(fid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_COMPLETED);


    OBJECT_BLKS_RESULT blk_result;
    _client->GetObjectBlksInfo(blk_result, fid);
    EXPECT_EQ(blk_result.Result, BFSS_RESULT::type::BFSS_SUCCESS);
    EXPECT_EQ(blk_result.ObjectBlksInfo.ObjectInfo.ObjectSize, size);
    EXPECT_TRUE(blk_result.ObjectBlksInfo.Blks.size() >= (size/blk_size));

    OBJECT_INFO_RESULT obj_result;
    _client->GetObjectInfo(obj_result, fid);
    EXPECT_EQ(obj_result.Result, BFSS_RESULT::type::BFSS_SUCCESS);
    EXPECT_EQ(obj_result.ObjectInfo.ObjectSize, size);
    EXPECT_FALSE(obj_result.ObjectInfo.Hash.empty());
    EXPECT_EQ(obj_result.ObjectInfo.Complete,   true);
    EXPECT_EQ(obj_result.ObjectInfo.ObjectTag,  "test-disordered");

    BFSS_RESULT::type  del_result = _client->DeleteObject(fid);
    EXPECT_EQ(del_result, BFSS_RESULT::type::BFSS_SUCCESS);
    del_result = _client->DeleteObject(fid);
    EXPECT_EQ(del_result, BFSS_RESULT::type::BFSS_NOTFOUND);

    /// compare hash..
    std::string cmp = gen_oid();
    result = _client->CreateObject(cmp, size, 0, "test-cmp");
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->Write(cmp, 0, begin_data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->Write(cmp, begin_data.length(), mid_data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->Write(cmp, size - end_size, end_data);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    result = _client->CompleteObject(cmp);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    OBJECT_INFO_RESULT cmp_result;
    _client->GetObjectInfo(cmp_result, cmp);
    EXPECT_EQ(cmp_result.Result, BFSS_RESULT::type::BFSS_SUCCESS);
    EXPECT_EQ(cmp_result.ObjectInfo.ObjectSize, size);
    EXPECT_EQ(cmp_result.ObjectInfo.Hash, obj_result.ObjectInfo.Hash);
    del_result = _client->DeleteObject(cmp);
    EXPECT_EQ(del_result, BFSS_RESULT::type::BFSS_SUCCESS);
}
void BFSS_API_Test_Exceptions::SetUpTestCase() {
    char *pszC = getenv("GTEST_BFSS_C");
    int c = 15;  if (pszC) c = atoi(pszC);/* read/write   chip size      ->  2^c  15=>32K  20=>1M*/
    std::cout << "export GTEST_BFSS_C=" << c << std::endl;
    rw_chip_size = (1<<c);
};
TEST_F(BFSS_API_Test_Exceptions, Write_BigFile) {
    // std::cout << "./BFSS_API_Test --gtest_filter=BFSS_API_Test_Exceptions.Write_BigFile --gtest_repeat=1000 --gtest_break_on_failure" << std::endl;
    string fid = gen_oid();

    std::string data = fid;
    char c_fill = rand() % 26 + 'A';
    data.resize(rw_chip_size, c_fill);

    int32_t size = object_max_size;
    BFSS_RESULT::type result = _client->CreateObject(fid, size, 0, "test-BIG");
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    int32_t offset = 0;
    while (offset < size) {
        result = _client->Write(fid, offset, data);
        offset += rw_chip_size;
    }
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

    result = _client->CompleteObject(fid);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
}

TEST_F(BFSS_API_Test_Exceptions, Write_File) {

    char *psz_size = getenv("GTEST_BFSS_FILE_SS");
    char *psz_tt_g = getenv("GTEST_BFSS_FILE_TG");

    int32_t ssize = _1M_SIZE;  if (psz_size) ssize = atoi(psz_size);/* 单个文件平均大小 */
    int64_t tsize = 2;         if (psz_tt_g) tsize = atoi(psz_tt_g);/* 连续写入的总大小G */
    static bool output = false;
    if (!output) {
        std::cout << "export GTEST_BFSS_FILE_SS=" << ssize << "  (Bytes)" << std::endl;
        std::cout << "export GTEST_BFSS_FILE_TG=" << tsize << "           (GBytes)" << std::endl;
        std::cout << "./BFSS_API_Test --gtest_filter=BFSS_API_Test_Exceptions.Write_File --gtest_repeat=10 --gtest_break_on_failure" << std::endl;
        output = true;
    }

    tsize *= (int64_t)_1G_SIZE;
    while (tsize > 0) {
        string fid = gen_oid();
        int32_t file_size = ssize / 2 + rand() % ssize;

        BFSS_RESULT::type result = _client->CreateObject(fid, file_size, 0, "test-continue-write");
        EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

        std::string data = fid;
        char c_fill = file_size % 26 + 'A';
        data.resize(rw_chip_size, c_fill);

        int32_t offset = 0;
        while (offset < file_size) {
            if ((file_size - offset) < rw_chip_size) {
                data.resize(file_size - offset);
            }
            result = _client->Write(fid, offset, data);
            offset += rw_chip_size;
        }
        EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);

        result = _client->CompleteObject(fid);
        EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
        tsize -= file_size;
    }
}