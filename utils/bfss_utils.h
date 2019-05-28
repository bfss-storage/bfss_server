//
// Created by han on 19-2-7.
//

#ifndef BFSS_ALL_UTILS_H
#define BFSS_ALL_UTILS_H

#include <string>
#include <cstdint>
#include <tuple>
#include <chrono>
#include <mutex>
#include <memory>

#include <assert.h>
#include <bfss_config.h>

#include <boost/serialization/serialization.hpp>

namespace bfss {

    struct oid_blk_info {
        int32_t BeginIndex = 0;
        int32_t BeginOffset = 0;
        int32_t Size = 0;
        int32_t Flags = 0;
    };

    class blk_cache_info{
    public:
        explicit blk_cache_info(int64_t _size) : size(_size){}
        int64_t get_size(){ return size; }
        template<size_t _BLK_SIZE>
        int get_count(){ return size / _BLK_SIZE; }
        char *get_ptr(){return data;}
        template<typename _T,int32_t _BLK_SIZE = sizeof(_T)>
        _T *get_blk(int _index){assert((size / _BLK_SIZE) > _index);return (((_T*)data) + _index);}
        friend std::shared_ptr<blk_cache_info> alloc_cache(int64_t min, int64_t max);
    private:
        int64_t size;
        char data[1];
    };

    unsigned int crc32(const unsigned char *_buf, size_t _size);

    bool check_oid(const std::string &_oid);

    template<int __align=blk_align_size>
    int64_t data_align(int64_t __size){
        return (__size - 1u + __align) & -__align;
    }

    template<int __align=blk_align_size>
    int32_t data_align(int32_t __size){
        return (__size - 1u + __align) & -__align;
    }

    template<int __align=blk_align_size>
    int64_t data_align_x(int64_t __size){
        int64_t __append = __size % __align;
        if(__append) __append = __align - __append;
        return __size + __append;
    }

    template<int __align=blk_align_size>
    inline int64_t pos_align(int64_t __pos)
    {
        return (__pos & ~(__align-1));
    }

    template<int __align=blk_align_size>
    inline int32_t pos_align(int32_t __pos)
    {
        return (__pos & ~(__align-1));
    }

    template<int __align=blk_align_size>
    int64_t pos_align_x(int64_t  __size) {
        int64_t  __append = __size % __align;
        if(__append < 0) return __size + -(__align + __append);
        return __size - __append;
    }
    
    inline int64_t get_blk_hash_node_key(const bfss::oid_blk_info *fbi){
        return ((int64_t)fbi->BeginIndex << 32) | fbi->BeginOffset;
    }
   
    template<typename _list_t, typename _value_or_iterator_t,typename _key_t>
    inline bool has_lockup(const _list_t & _list,const _key_t &_key,_value_or_iterator_t & __it){
        auto _it = const_cast<_list_t&>(_list).find(_key);
        if(_it != _list.end()){
            __it = _it;
            return true;
        }
        return false;
    }

    template<typename _list_t, typename _key_t>
    inline bool has_lockup(_list_t & _list,const _key_t &_key){
        return (_list.end() == _list.find(_key)) ? false : true;
    }

    inline std::tuple<std::string,int> parse_address(const std::string &_address,
            const std::string && _default_addr = "localhost", const int _default_port = 9090){

        auto _pos = _address.find(':');
        auto _end_pos = _address.begin() - _address.end();
        if( _pos == std::string::npos){
            return std::make_tuple(_address, _default_port);
        } else if(_pos == 0){
            return std::make_tuple(_default_addr, (int)std::strtol(_address.substr(1,_end_pos).c_str(), nullptr, 10));
        }
        return std::make_tuple(_address.substr(0,_pos), (int)std::strtol(_address.substr(_pos+1,_end_pos).c_str(), nullptr, 10));
    }

    inline std::tuple<std::string,std::tuple<std::string,int> > parse_uri(const std::string &_uri,
            const int _default_port = 9090){

        size_t _pos = _uri.find("://");
        size_t _end_pos = _uri.begin() - _uri.end();
        if( _pos == std::string::npos || _pos == 0 || (_pos + 3u) == _end_pos){
            throw std::exception();
        }
        auto _scheme = _uri.substr(0,_pos);
        auto _args_pos = _uri.find('?',_pos);
        if(_args_pos != std::string::npos){
            _end_pos = _args_pos;
        }
        return std::make_tuple(_scheme, parse_address(_uri.substr(_pos + 3,_end_pos)));
    }

    inline int get_blk_end_offset(int64_t _pos){
        assert(_pos >= 0);
        return int(_pos - pos_align_x<blk_size>(_pos));
    }
    int64_t get_oid_begin_pos(int _ob_index, int _ob_off);

    inline void check_assert_bp(int64_t  _v){
        assert(_v >= 0);
        assert(data_align(_v) == _v);
    }

    inline int get_blk_end_offset(int _b_index, int _b_off,int64_t _size){
        assert(_b_off < blk_size);
        check_assert_bp(_b_off);
        assert(data_align(_size) == _size);
        assert(_b_index >= 0);
        auto _pos = get_oid_begin_pos(_b_index,_b_off) + _size;
        return int(_pos - pos_align_x<blk_size>(_pos));
    }

    inline int get_blk_begin_offset(int64_t _pos,bool _check = true){
        if(_check) check_assert_bp(_pos);
        return int(_pos - pos_align_x<blk_size>(_pos));
    }

    inline int get_blk_index(int _b_index, int _b_off,int64_t _size){
        assert(_b_off < blk_size);
        check_assert_bp(_b_off);
        assert(data_align(_size) == _size);
        return _b_index + int(pos_align_x<blk_size>(_b_off + _size) / blk_size);
    }

    inline int get_blk_index(int _b_index, int _b_off,int32_t _size){
        return get_blk_index(_b_index,_b_off,(int64_t)_size);
    }

    inline int get_blk_index(int64_t _pos,bool _check = true){
        if(_check) check_assert_bp(_pos);
        return int(pos_align_x<blk_size>(_pos) / blk_size);
    }

    inline int64_t get_oid_size(int _ob_index, int _ob_off, int _oe_index, int _oe_off){
        assert(_oe_index >= _ob_index);
        assert(_ob_off < blk_size);
        assert(_oe_off < blk_size);
        check_assert_bp(_ob_off);
        check_assert_bp(_oe_off);
        assert(_oe_index >= 0);
        assert(_ob_index >= 0);
        assert(((int64_t)_oe_index * blk_size + _oe_off) >= ((int64_t)_ob_index * blk_size + _ob_off));
        return ((int64_t)_oe_index * blk_size + _oe_off) - ((int64_t)_ob_index * blk_size + _ob_off);
    }

    inline int64_t get_oid_begin_pos(int _ob_index, int _ob_off) {
        assert(_ob_off <= blk_size);
        return ((int64_t)_ob_index * blk_size + _ob_off);
    }

    inline int64_t get_oid_begin_pos(int _ob_index, int _ob_off, int _oe_index, int _oe_off){
        assert(_oe_index >= _ob_index);
        assert(_ob_off < blk_size);
        assert(_oe_off < blk_size);
        check_assert_bp(_ob_off);
        check_assert_bp(_oe_off);
        assert(_oe_index >= 0);
        assert(_ob_index >= 0);
        assert(((int64_t)_oe_index * blk_size + _oe_off) >= ((int64_t)_ob_index * blk_size + _ob_off));
        return get_oid_begin_pos(_ob_index, _ob_off);
    }

    inline int64_t get_oid_end_pos(int _oe_index, int _oe_off){
        assert(_oe_off < blk_size);
        assert(_oe_index >= 0);
        check_assert_bp(_oe_off);
        return ((int64_t)_oe_index * blk_size + _oe_off);
    }

    inline int64_t get_oid_end_pos(int _ob_index, int _ob_off, int _oe_index, int _oe_off){
        assert(_oe_index >= _ob_index);
        check_assert_bp(_ob_off);
        check_assert_bp(_oe_off);
        assert(((int64_t)_oe_index * blk_size + _oe_off) >= ((int64_t)_ob_index * blk_size + _ob_off));
        return get_oid_end_pos(_oe_index,_oe_off);
    }

    inline int64_t get_oid_end_pos(int _ob_index, int _ob_off, int64_t _size){
        assert(data_align(_size) == _size);
        check_assert_bp(_ob_off);
        assert(_ob_off < blk_size);
        return ((int64_t)_ob_index * blk_size + _ob_off + _size);
    }

    inline std::time_t get_time_stamp() {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now());
        auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        return tmp.count();
    }

    template<typename _string_t,typename _string1_t>
    const _string1_t& bin2hex(const _string_t& _str,_string1_t &_out){
        const char * _hex = "0123456789abcdef";
        _out.resize(_str.length() * 2);
        for (size_t __i = 0; __i < _str.length(); ++__i) {
            uint8_t __c = (uint8_t&)_str[__i];
            _out[__i*2] = _hex[(__c & 0xf0) >> 4];
            _out[(__i*2)+1] = _hex[(__c & 0xf)];
        }
        return _out;
    }

    template<typename _string_t,typename _string1_t>
    const _string1_t& hex2bin(const _string_t& _str,_string1_t& _out) {
        static const unsigned char xx_id[256] = {
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 1, 2, 3, 4, 5, 6, 7,  8, 9, 0, 0, 0, 0, 0, 0,
                0,10,11,12,13,14,15, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0,10,11,12,13,14,15, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
        };

        auto __out_len = _str.length()/2;
        _out.resize(__out_len);

        for (size_t __i = 0; __i < __out_len; ++__i) {
            _out[__i] = (xx_id[(uint8_t&)_str[__i*2]] << 4) | xx_id[(uint8_t&)_str[__i*2+1]];
        }

        return _out;
    }

    template <typename T>
    inline int64_t calc_object_size(const T & _info) {
        return get_oid_size(_info.BeginIndex, _info.BeginOffset, _info.EndIndex, _info.EndOffset);
    }
    std::shared_ptr<blk_cache_info> alloc_cache(int64_t min = 0xffff, int64_t max = (size_t) -1);
    void free_cache(intptr_t _ptr);

    inline int64_t str2size(const std::string &_size)
    {
        char *_pend;
        int64_t _ll_szie = std::strtoll(_size.c_str(),&_pend,10);
        switch (*_pend){
            case 'K':
            case 'k':
                _ll_szie *= 1024;
                break;
            case 'M':
            case 'm':
                _ll_szie *= _1M_SIZE;
                break;
            case 'G':
            case 'g':
                _ll_szie *= (int64_t)_1G_SIZE;
                break;
            case 'T':
            case 't':
                _ll_szie *= ((int64_t)_1G_SIZE * 1024);
                break;
            case 'P':
            case 'p':
                _ll_szie *= ((int64_t)_1G_SIZE * 1024 * 1024);
                break;
            default:
                assert(*_pend == 0);
                break;
        }
        return _ll_szie;
    }
    template<typename _cache_t>
    inline void copy_cache(_cache_t &_des,const _cache_t &_org){
        _des.VolumeId = _org.VolumeId;
        _des.BeginIndex = _org.BeginIndex;
        _des.BeginOffset = _org.BeginOffset;
        _des.Size = _org.Size;
        _des.Time = _org.Time;
        _des.Complete = _org.Complete;
        _des.oid = _org.oid;
        _des.ctx = _org.ctx;
        _des.Flag = _org.Flag;
        _des.head = _org.head;
        _des.hash = _org.hash;
        _des.tag = _org.tag;
    }
    template<typename _cache_t>
    inline bool compp_cache(const _cache_t &_des,const _cache_t &_org){
        return _des.VolumeId == _org.VolumeId&&
        _des.BeginIndex == _org.BeginIndex&&
        _des.BeginOffset == _org.BeginOffset&&
        _des.Size == _org.Size&&
        _des.Time == _org.Time&&
        _des.Flag == _org.Flag&&
        _des.Complete == _org.Complete&&
        _des.oid == _org.oid&&
        _des.ctx == _org.ctx&&
        _des.head == _org.head&&
        _des.hash == _org.hash&&
        _des.tag == _org.tag;
    }
    template <typename _cache_t>
    class cache_perception{
    public:
        cache_perception(_cache_t &_cache): cache_(_cache),cache_old_(_cache.oid,_cache.manage){
            copy_cache(cache_old_,cache_);
        }
        ~cache_perception(){
            if(!compp_cache(cache_,cache_old_)){
                cache_.update();
            }
        }
    protected:
        _cache_t &cache_;
        _cache_t cache_old_;

    };
}

#endif //BFSS_ALL_UTILS_H
