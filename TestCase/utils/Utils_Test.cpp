//
// Created by han on 19-2-9.
//

#include "Utils_Test.h"
#include  <type_traits>
#include <memory>
#include <cstdio>
#include <bfss_utils.h>
#include <bfss_config.h>

TEST(UtilTest, Test_bfss_align){

    EXPECT_EQ(bfss::data_align(0),0);
    EXPECT_EQ(bfss::data_align(1),16);
    EXPECT_EQ(bfss::data_align(15),16);
    EXPECT_EQ(bfss::data_align(16),16);
    EXPECT_EQ(bfss::data_align(17),32);
    EXPECT_EQ(bfss::pos_align(17),16);
    EXPECT_EQ(bfss::pos_align(12),0);
    EXPECT_EQ(bfss::pos_align(16),16);
    EXPECT_EQ(bfss::pos_align(32),32);
    auto x = bfss::data_align_x<5*_1M_SIZE>(_1M_SIZE + 1024);
    EXPECT_EQ(x,(int)5*_1M_SIZE);
    x = bfss::data_align_x<5*_1M_SIZE>(_1M_SIZE * 5 + 1024);
    EXPECT_EQ(x,10 * _1M_SIZE);
}

TEST(UtilTest, Test_bfss_crc32){

    EXPECT_EQ(bfss::crc32((const unsigned char*)"123",3),0x884863D2);
    EXPECT_EQ(bfss::crc32((const unsigned char*)"Test_bfss_crcT",14),0x7BA45F3B);
}
//TEST(UtilTest, Test_bfss_bin2hex){
//    std::string out;
//    EXPECT_EQ(bfss::bin2hex("abc",out),"616263");
//    EXPECT_EQ(bfss::bin2hex("abd",out),"616264");
//    EXPECT_EQ(bfss::bin2hex("01234abdcfe98765"), "30313233346162646366653938373635");
//}
//TEST(UtilTest, Test_bfss_hex2bin){
//    EXPECT_EQ(bfss::hex2bin("616263"),"abc");
//    EXPECT_EQ(bfss::hex2bin("616264"),"abd");
//    EXPECT_EQ(bfss::hex2bin("30313233346162646366653938373635"),"01234abdcfe98765");
//}
TEST(UnitTest, Test){
    auto BeginIndex = 10;
    auto BeginOffset = _1M_SIZE;
    auto EndIndex = 11;
    auto EndOffset = 4 * _1M_SIZE;
    /*
     * 断言检测数据
     */
    auto alloc_size = 4 *_1M_SIZE;
    assert((((EndIndex - BeginIndex) * blk_size) + (BeginOffset + EndOffset)) >= alloc_size);
    auto EndPosition = (EndIndex * blk_size + EndOffset);
    auto BeginIndex1 = (EndPosition - alloc_size) / blk_size;
    EXPECT_EQ(BeginIndex1,11);
    auto x= bfss::pos_align_x<blk_size>(-(5*_1M_SIZE)+(-10));
    EXPECT_EQ(x, -(blk_size*2));
    x= bfss::pos_align_x<blk_size>(-(5*_1M_SIZE)+1);
    EXPECT_EQ(x, -(blk_size));
    x= bfss::pos_align_x<blk_size>(-(5*_1M_SIZE)+1);
    EXPECT_EQ(x, -(blk_size));
//    x= bfss::pos_align<int64_t ,blk_size>(5*_1M_SIZE + 10);
//    EXPECT_EQ(x, 5*_1M_SIZE);
//    EXPECT_EQ(bfss::check_oid("asdfghjklQWERTYUIOASDFGHJKLqwertyuiopZXCVBNMzxcvbnm_-.123456789"),true);
    EXPECT_EQ(bfss::check_oid("asdfghjklQWERTYUIOASDFGHJKLqwertyuiopZXCVBNMzxcvbnm_-.*12356789"),false);
    EXPECT_EQ(bfss::check_oid("asdfghjklQWERTYUIOASDFGHJKLqwertyuiopZXCVBNMzxcvbnm_-.阿"),false);
    EXPECT_EQ(bfss::check_oid("asdfghjklQWERTYUIOASDFGHJKLqwertyuiopZXCVBNMzxcvbnm_-.～！@#￥*（）——"),false);
    EXPECT_EQ(bfss::get_blk_end_offset(1,0x80,-0x80),0);
    EXPECT_EQ(bfss::get_blk_end_offset(518,994096,bfss::data_align(967896)),1962000);

}


TEST(UnitTest, Tets_freeM){
    auto a = bfss::alloc_cache(_1M_SIZE*5,(size_t)_1M_SIZE*1024*16);
    EXPECT_EQ((bool)a, true);
    EXPECT_EQ(a->get_size() > _1M_SIZE*5 && a->get_size() <= (size_t)_1M_SIZE*1024*16, true);
}


//TEST(UnitTest, Test_oid_cache_of_index) {
//    /*>>>./Utils_Test --gtest_filter=UnitTest.T_oid_cache_of_index --gtest_repeat=10000  > oid_test.txt
//     *>>>tail -f oid_test.txt |grep SUMMARY           */
//    std::map<int, int>    all_index_count;
//
//    int64_t n_test_oid = oid_cache_size;
//    for (int i=0; i<n_test_oid; i++) {
//
//        std::string oid = gen_oid();
//        int index = bfss::oid_cache_of_index(oid);
//
//        if (all_index_count.find(index) != all_index_count.end()) {
//            //std::cout << "repeated index ot oid: " << oid << std::endl;
//            all_index_count[index] += 1;
//        } else {
//            all_index_count[index] = 0;
//        }
//    }
//
//    int duplicated_index_count = 0;
//    int max_duplicated_times = 0;
//    for (auto it = all_index_count.begin() ; it != all_index_count.end(); ++it) {
//        int index_count = it->second;
//        if (index_count > 1) {
//            duplicated_index_count ++;
//        }
//        if (max_duplicated_times < index_count) {
//            max_duplicated_times = index_count;
//        }
//    }
//    std::cout << "[SUMMARY]===" << n_test_oid << " oids, get " << all_index_count.size() << " index===";
//    std::cout << " duplicated_index_count=" << duplicated_index_count << "  max_duplicated_times=" << max_duplicated_times << std::endl;
//
//}

#include <bffs_memcache.h>

TEST(UnitTest, TestM){
    std::string _s ="memc://localhost:11211";
    bfss::bfss_memcache _a(_s);

    std::string _k ="12346";
    std::string _v;
    bool _r = _a.get_cache(_k,_v);
    EXPECT_TRUE(_r  && _v == "1234" || !_r );
    _v = "1234";
    _a.set_cache(_k,_v);
    _v = "";
    EXPECT_EQ(_a.get_cache(_k,_v), true);

    EXPECT_EQ(_v, "1234");
    _k = "aaaaaaaaaaaaaaaaaaaaaa";

    EXPECT_EQ(_a.try_lock(_k), true);
    EXPECT_EQ(_a.try_lock(_k), false);
    _a.unlock(_k);
    EXPECT_EQ(_a.try_lock(_k), true);
    _a.unlock(_k);
    EXPECT_EQ(_a.try_lock(_k), true);
    _a.unlock(_k);
    EXPECT_EQ(_a.try_lock(_k), true);
    _a.unlock(_k);
    _a.lock(_k);
    _a.unlock(_k);

}
#include <boost/dynamic_bitset.hpp>

TEST(UnitTest, Test_valid_fid ) {

    static const bool __valid_fid[255] = {
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,true ,true ,false,
            true ,true ,true ,true ,true ,true ,true ,true ,
            true ,true ,false,false,false,false,false,false,
            false,true ,true ,true ,true ,true ,true ,true ,
            true ,true ,true ,true ,true ,true ,true ,true ,
            true ,true ,true ,true ,true ,true ,true ,true ,
            true ,true ,true ,false,false,false,false,true ,
            false,true ,true ,true ,true ,true ,true ,true ,
            true ,true ,true ,true ,true ,true ,true ,true ,
            true ,true ,true ,true ,true ,true ,true ,true ,
            true ,true ,true ,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false,false,false,
            false,false,false,false,false,false
    };

    for (int i=0; i<255; i++)
    {
        if ( __valid_fid[i] ) {
            std::cout << (char)i ;
        }
    }
    std::cout << std::endl;
    boost::dynamic_bitset<uint64_t> dynamic_bitset(10000);
    dynamic_bitset[238] = true;

    std::cout << dynamic_bitset << std::endl;
    EXPECT_EQ(__valid_fid['.'], true);
    EXPECT_EQ(__valid_fid['-'], true);
    EXPECT_EQ(__valid_fid['='], false);
    EXPECT_EQ(__valid_fid['/'], false);

};
