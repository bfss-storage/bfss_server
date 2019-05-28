//
// Created by han on 19-2-7.
//

#ifndef BFSS_ALL_CONFIG_H
#define BFSS_ALL_CONFIG_H

#define oid_cache_size      10000
#define _1M_SIZE            (1024*1024)
#define blk_size            (5*_1M_SIZE)
#define oid_max_len         128
#define tag_max_len         128
#define _1G_SIZE            (1024*_1M_SIZE)
#define chip_max_size       _1M_SIZE
#define blk_align_size      16
#define cache_size          (12*blk_size)
#define to_disk_time_out    (1000*60)
#define object_max_size     (_1G_SIZE+(500*_1M_SIZE))
#define object_head_len     16
#endif //BFSS_ALL_CONFIG_H
