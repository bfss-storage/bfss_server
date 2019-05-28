//
// Created by han on 19-2-7.
//

#ifndef BFSS_ALL_BLKDEV_H
#define BFSS_ALL_BLKDEV_H

#include <stdint.h>
#include <boost/serialization/singleton.hpp>

namespace bfss{

    class BlkDevice {
    private:
        int         _fd;
        uint64_t    _size;

        uint32_t    _blk_count;
        uint32_t    _reserved; /* reserved 1 or 2 block */

        std::mutex  _mutex;

        int32_t     _bit_bytes;
        char*       _bits;
        bool init_bits();
        bool is_writed(int blk_index);
        bool set_writed(int blk_index);
        bool write_bits();

    public:
        BlkDevice() = default;
        ~BlkDevice() {  close(); }

        bool open(const char *__dst);
        void close();

        uint32_t blocks() const {return (_blk_count - _reserved);}

        bool readblk(int blk_index, void *__buf);
        bool writeblk(int blk_index, const void *__buf);

    };

    typedef boost::serialization::singleton<BlkDevice> singleton_blk_device;
#define _s_blk_device      singleton_blk_device::get_mutable_instance()
#define _c_blk_device      singleton_blk_device::get_const_instance()
}

#endif //BFSS_ALL_BLKDEV_H
