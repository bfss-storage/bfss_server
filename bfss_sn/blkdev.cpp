//
// Created by han on 19-2-7.
//

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <unistd.h>

#include <bfss_base.h>
#include <bfss_sn_types.h>
#include <bfss_exception.h>
#include "blkdev.h"

namespace bfss {

    bool BlkDevice::open(const char *__dst) {
        struct stat64 st;
        stat64(__dst, &st);
        if (!S_ISBLK(st.st_mode)) {
            return false;
        }
        _fd = ::open(__dst, O_RDWR); /*eg. /dev/sdb*/
        if (_fd == -1) {
            return false;
        }
        if (ioctl(_fd, BLKGETSIZE64, &_size) == -1) {
            close();
            return false;
        }

        _blk_count = (uint32_t)(_size / blk_size);
        _reserved = 2;  /* ***reserved 2 block for status management. */
        return init_bits();
    }

    void BlkDevice::close() {
        write_bits();
        if (_bits) {
            delete _bits;
            _bits = nullptr;
        }

        if (_fd != -1) {
            ::close(_fd);
            _fd = -1;
        }
    }

    bool BlkDevice::readblk(int blk_index, void *__buf) {
        std::lock_guard _lock(_mutex);
        if ((blk_index > _blk_count) || (_fd == -1)) {
            return false;
        }
        if (!is_writed(blk_index)) {
            memset(__buf, 0, blk_size);
            return true;
        }
        __off64_t _offset = __off64_t(blk_index + _reserved) * blk_size;
        return blk_size == pread64(_fd, __buf, blk_size, _offset);
    }

    bool BlkDevice::writeblk(int blk_index, const void *__buf) {
        std::lock_guard _lock(_mutex);
        if ((blk_index > _blk_count) || (_fd == -1)) {
            return false;
        }
        set_writed(blk_index);

        __off64_t _offset = __off64_t(blk_index + _reserved) * blk_size;
        return blk_size == pwrite64(_fd, __buf, blk_size, _offset);
    }


    const int _mask_len = 8;
    const unsigned char _mask[] = {'B', 'F', 'S', 'S', '-', 'S', 'N', '\0'};
    bool BlkDevice::init_bits() {
        char _test[_mask_len] = {0};
        ssize_t read_size = pread64(_fd, _test, _mask_len, 0);
        if (read_size != _mask_len) {
            return false;
        }

        _bit_bytes = bfss::data_align((int32_t )_blk_count/8);
        assert(_bit_bytes < (blk_size*_reserved - _mask_len));
        if(_bit_bytes >= (blk_size*_reserved - _mask_len)){
            return false;
        }
        _bits = new char[_bit_bytes];

        if (memcmp(_test, _mask, _mask_len) != 0) {
            memset(_bits, 0, _bit_bytes);
            return (_mask_len == pwrite64(_fd, _mask, _mask_len, 0));
        } else {
            __off64_t _offset = _mask_len;
            return _bit_bytes == pread64(_fd, _bits, _bit_bytes, _offset);
        }
    }
    bool BlkDevice::is_writed(int blk_index) {
        int _i = blk_index /8;
        auto _v = uint8_t(1 << (blk_index%8));
        return (_bits[_i] & _v) == _v;
    }
    bool BlkDevice::set_writed(int blk_index){
        int _i = blk_index /8;
        auto _v = uint8_t(1 << (blk_index%8));
        _bits[_i] |= _v;
        return (_bits[_i] & _v) == _v;
    }
    bool BlkDevice::write_bits(){
        std::lock_guard _lock(_mutex);
        if (_fd == -1) {
            return false;
        }
        return _bit_bytes == pwrite64(_fd, _bits, _bit_bytes, _mask_len);
    }
}