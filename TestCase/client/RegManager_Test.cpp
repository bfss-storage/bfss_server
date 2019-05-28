

#include "RegManager_Test.h"
using namespace bfss;

/* 预设 不同大小的卷 volumeId */
static std::map<int32_t, int32_t> _VolumeId_BlkCount_map;

#define TEST_WITH_REAL_VOLUMES        //为了后续操作的正确性，不使用虚构的卷信息注册

void REGM_TEST::SetUpTestCase()
{
    //  启动服务

    // 初始化卷信息 只确定卷大小 卷IP地址由卷id构造产生
#ifndef TEST_WITH_REAL_VOLUMES
    int minCount = 256;
    for (int i=0; i<16; i++) {

        int32_t vol = 127 + i;          /* 卷设备 IP地址 */
        int32_t count = minCount << i;  /* 卷大小 */
        _VolumeId_BlkCount_map[vol] = count;

        std::cout  << " volume index "<< vol << " size " << count << std::endl;
    }
    EXPECT_TRUE(_VolumeId_BlkCount_map.size() > 10);
#endif
}

void REGM_TEST::TearDownTestCase()
{

    // 停止服务
}

void REGM_TEST::SetUp()
{
/* 环境变量控制运行时参数
export GTEST_BFSS_API_IP=127.0.0.1
export GTEST_BFSS_API_PORT=9092
 */
    char *pszIP   = getenv("GTEST_BFSS_REGM_IP");
    char *pszPORT = getenv("GTEST_BFSS_REGM_PORT");
    std::string ip("127.0.0.1");    if (pszIP)      ip = std::string(pszIP);
    int port = 9092;                if (pszPORT)  port = atoi(pszPORT);
    static bool test_output = false;
    if (!test_output) {
        std::cout << "export GTEST_BFSS_REGM_IP="   <<   ip << std::endl;
        std::cout << "export GTEST_BFSS_REGM_PORT=" << port << std::endl;
        test_output = true;
    }

    stdcxx::shared_ptr<TTransport> socket(new TSocket(ip.c_str(), port));
    stdcxx::shared_ptr<TTransport> transport(new TFramedTransport(socket));
    stdcxx::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    _client = new BFSS_REGMDClient(protocol);
    transport->open();

    EXPECT_TRUE(_client != 0);

}

void REGM_TEST::TearDown()
{
    _client->getInputProtocol()->getInputTransport()->close();
    delete _client;

    EXPECT_TRUE(1);
}


/*
  void ManageMessage(MESSAGE_REGM_RESULT& _return, const int32_t CmdId, const int32_t Param, const std::string& Data) = 0;
*/
const int16_t CMD_SN_ENABLE = 1;
const int16_t CMD_SN_DISABLE = 2;
TEST_F(REGM_TEST, msg_sn_enable) {
    /*> ./BFSS_RegM_TestClient --gtest_filter=REGM_TEST.msg_sn_enable   */
    int32_t v_id = 10002;
    char *pszVID = getenv("GTEST_BFSS_SN_ID");
    if (pszVID) {
        v_id = atoi(pszVID);
    }

    MESSAGE_REGM_RESULT msg;
    std::string data('x', 10);
    _client->ManageMessage(msg, CMD_SN_ENABLE, v_id, data);
    EXPECT_EQ(msg.Result, BFSS_RESULT::type::BFSS_SUCCESS);
}
TEST_F(REGM_TEST, msg_sn_disable) {
    /*> ./BFSS_RegM_TestClient --gtest_filter=REGM_TEST.msg_sn_disable  */
    int32_t v_id = 10002;
    char *pszVID = getenv("GTEST_BFSS_SN_ID");
    if (pszVID) {
        v_id = atoi(pszVID);
    }

    MESSAGE_REGM_RESULT msg;
    std::string data;
    _client->ManageMessage(msg, CMD_SN_DISABLE, v_id, data);
    EXPECT_EQ(msg.Result, BFSS_RESULT::type::BFSS_SUCCESS);
}

/*
  BFSS_RESULT::type RegisterVolume(const REGISTER_VOLUME_INFO& VolumeInfo) = 0;
*/
TEST_F(REGM_TEST, RegisterVolume) {

#ifndef TEST_WITH_REAL_VOLUMES
    auto get_ip = [](int32_t ip4) {
        ostringstream oss;
        oss << "192.168.141." << ip4;
        return oss.str();
    };
    auto it = _VolumeId_BlkCount_map.begin();
    for (; it != _VolumeId_BlkCount_map.end(); ++it) {

        REGISTER_VOLUME_INFO VolumeInfo;

        VolumeInfo.Uri = get_ip(it->first);
        VolumeInfo.VolumeId = it->first;
        VolumeInfo.BlkCount = it->second;
        VolumeInfo.Desc = VolumeInfo.Uri;

        BFSS_RESULT::type result = _client->RegisterVolume(VolumeInfo);
        EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
    }
#endif

    //注册本地真实可用的SN节点
    REGISTER_VOLUME_INFO VolumeInfo;

    VolumeInfo.Uri = "127.0.0.1";
    VolumeInfo.VolumeId = 10002;
    VolumeInfo.BlkCount = 100;
    VolumeInfo.Desc = VolumeInfo.Uri;

    BFSS_RESULT::type result = _client->RegisterVolume(VolumeInfo);
    EXPECT_EQ(result, BFSS_RESULT::type::BFSS_SUCCESS);
}


/*
  void GetVolumeInfo(VOLUME_RESULT& _return, const int32_t VolumeId) = 0;

*/
TEST_F(REGM_TEST, GetVolumeInfo) {

    EXPECT_NE(_VolumeId_BlkCount_map.size(), 0);

#ifndef TEST_WITH_REAL_VOLUMES
    auto it = _VolumeId_BlkCount_map.begin();
    for ( ; it != _VolumeId_BlkCount_map.end(); ++it) {

        const int32_t & VolumeId = it->first;

        VOLUME_RESULT result;
        _client->GetVolumeInfo(result, VolumeId);
        EXPECT_EQ(result.Result, BFSS_RESULT::type::BFSS_SUCCESS);
        EXPECT_EQ(result.Info.VolumeId, VolumeId);
        EXPECT_EQ(result.Info.BlkCount, it->second);
    }
#endif

    VOLUME_RESULT result;
    int32_t  VolumeId = 10002;
    _client->GetVolumeInfo(result, VolumeId);
    EXPECT_EQ(result.Result, BFSS_RESULT::type::BFSS_SUCCESS);
    EXPECT_EQ(result.Info.VolumeId, VolumeId);
}



/*
  void GetAllVolumeInfo(std::vector<VOLUME_RESULT> & _return) = 0;

*/

TEST_F(REGM_TEST, GetAllVolumeInfo) {

    std::vector<VOLUME_INFO> result;

    _client->GetAllVolumeInfo(result);

    EXPECT_NE(result.size(), 0);
}

/*

  void AllocateBlks(ALLOCATE_BLKS_RESULT& _return, const int32_t size) = 0;

*/

std::vector<ALLOCATE_BLKS_RESULT> alloc_blks_vector;


TEST_F(REGM_TEST, AllocateBlks) {

    int size = 0;
    for (int i=0; i< 100 ; ++i) {

        if (i<24) {
            size = 1 << i;            // 大小 在 范围内时 以2的指数递增
        } else {
            // 大小 超过范围后 获得1024范围内的随机的大小
            size = rand() % (1<<16) +  rand()%32 + 2;
        }

        ALLOCATE_BLKS_RESULT result;
        _client->AllocateBlks(result, size);

        EXPECT_EQ(result.Result,  BFSS_RESULT::type::BFSS_SUCCESS);
        alloc_blks_vector.push_back(result);
    }
}


/*
    BFSS_RESULT::type AddBlkChip(const ALLOCATE_BLKS_INFO& Allocate_Blks_info) = 0;
*/
TEST_F(REGM_TEST, AddBlkChip) {

    size_t chip_count = alloc_blks_vector.size();
    for (int i = 0; i < chip_count; i++) {
        ALLOCATE_BLKS_RESULT& chip = alloc_blks_vector[i];
        if (i % 4 != 0) {
            continue; /* 先回收1/4 */
        }

        BFSS_RESULT::type result = _client->AddBlkChip(chip.BlksInfo);
        EXPECT_EQ(result ,  BFSS_RESULT::type::BFSS_SUCCESS);
    }

    for (int i = 0; i < chip_count; i++) {
        ALLOCATE_BLKS_RESULT& chip = alloc_blks_vector[i];
        if (i % 4 != 1) {
            continue; /* 回收其后相邻的块 */
        }

        BFSS_RESULT::type result = _client->AddBlkChip(chip.BlksInfo);
        EXPECT_EQ(result ,  BFSS_RESULT::type::BFSS_SUCCESS);
    }

    for (int i = 0; i < chip_count; i++) {
        ALLOCATE_BLKS_RESULT& chip = alloc_blks_vector[i];
        if (i % 4 != 3) {
            continue; /* 回收其前相邻的块 */
        }

        BFSS_RESULT::type result = _client->AddBlkChip(chip.BlksInfo);
        EXPECT_EQ(result ,  BFSS_RESULT::type::BFSS_SUCCESS);
    }

    for (int i = 0; i < chip_count; i++) {
        ALLOCATE_BLKS_RESULT chip = alloc_blks_vector[i];
        if (i % 4 != 2) {
            continue;
        }

        chip.BlksInfo.BeginOffset += 13;  /* 修改参数 预期异常*/
        BFSS_RESULT::type result = _client->AddBlkChip(chip.BlksInfo);
        EXPECT_NE(result ,  BFSS_RESULT::type::BFSS_SUCCESS);
    }
}


TEST_F(REGM_TEST, ReAllocateBlks) {

    int size = 0;
    for (int i; i< 100 ; ++i) {

        if (i<24) {
            size = 1 << i;            // 大小 在 范围内时 以2的指数递增
        } else {
            // 大小 超过范围后 获得1024范围内的随机的大小
            size = rand() % (1<<16) +  rand()%32 + 2;
        }

        ALLOCATE_BLKS_RESULT result;
        _client->AllocateBlks(result, size);

        EXPECT_EQ(result.Result,  BFSS_RESULT::type::BFSS_SUCCESS);
        alloc_blks_vector.push_back(result);
    }
}

