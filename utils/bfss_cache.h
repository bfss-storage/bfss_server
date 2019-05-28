//
// Created by han on 19-3-30.
//

#ifndef BFSS_ALL_BFSS_CACHE_H
#define BFSS_ALL_BFSS_CACHE_H

#include <stdint.h>
#include <functional>
#include <boost/serialization/array.hpp>

#define object_hash_hex_len (32 /*SHA256_DIGEST_LENGTH*/ * 2)

namespace bfss {

    class bfss_memcache;
    template<typename _cache_t>
    class cache_perception;

    template <size_t _fix_buff_size = 64>
    class bfss_string{
    public:
        bfss_string(const bfss_string & string_): _size(string_._size), _buff(_fix_buff){
            resize(_size);
            memcpy(_buff,string_._buff,_size);
        }

        explicit bfss_string(const std::string & string_): _size(string_.size()), _buff(_fix_buff){
            resize(_size);
            memcpy(_buff,string_.c_str(),_size);
        }

        bfss_string(const char* & string_,size_t len_): _size(len_), _buff(_fix_buff){
            resize(_size);
            memcpy(_buff,string_,_size);
        }

        bfss_string(const char* string_): _size(0), _buff(_fix_buff){
            resize(strlen(string_));
            memcpy(_buff,string_,_size);
        }

        bfss_string(): _size(0), _buff(_fix_buff){}

        ~bfss_string(){
            if(_buff != _fix_buff){
                free(_buff);
            }
        }

        void resize(size_t size_,bool fill_ = true) {
            if (_size != size_) {
                char *olb_buff = _buff;
                if (size_ > _fix_buff_size) {
                    _buff = (char *) malloc(size_ + 1);
                } else {
                    _buff = _fix_buff;
                }
                if (fill_ && (_buff != olb_buff)) {
                    memcpy(_buff, olb_buff, std::min(_size, size_));
                }
                _buff[size_] = 0;
                _size = size_;
                if (olb_buff != _fix_buff) {
                    free(olb_buff);
                }
            }
        }

        const char *c_str(){
            return _buff;
        }

        const char *c_str() const {
            return _buff;
        }

        std::string to_string() const {
            return std::string(_buff, _size);
        }

        bool operator == (const char *str_) const {
            size_t len = strlen(str_);
            if (_size != len) {
                return false;
            }
            return memcmp(_buff, str_, std::min(len, _size)) == 0;
        }

        bool operator != (const char *str_) const {
            return !operator==(str_);
        }

        bool operator == (const std::string &str_) const {
            size_t len = str_.size();
            if (_size != len) {
                return false;
            }
            return memcmp(_buff, str_.c_str(), std::min(len, _size)) == 0;
        }

        bool operator != (const std::string &str_) const{
            return ! operator==(str_);
        }

        bool operator == (const bfss_string &str_) const {
            size_t len = str_._size;
            if (_size != len) {
                return false;
            }
            return memcmp(_buff, str_._buff, std::min(len, _size)) == 0;
        }

        bool operator != (const bfss_string &str_) const{
            return ! operator==(str_);
        }

        bfss_string& operator = (const bfss_string &string_) {
            _size = string_.size();
            resize(_size);
            memcpy(_buff, string_.c_str(), _size);
            return *this;
        }

        bfss_string& operator = (const std::string &string_) {
            _size = string_.size();
            resize(_size);
            memcpy(_buff, string_.c_str(), _size);
            return *this;
        }

        bfss_string& operator = (const char* string_) {
            _size = strlen(string_);
            resize(_size);
            memcpy(_buff, string_, _size);
            return *this;
        }

        template<typename string_t>
        bfss_string& operator + (const string_t &string_) {
            size_t olb_size = _size;
            resize(_size + string_.size());
            memcpy(_buff + olb_size, string_.c_str(), string_.size());
            return *this;
        }

        bfss_string& operator + (const char *string_) {
            size_t olb_size = _size;
            size_t len = strlen(string_);
            resize(_size + strlen(string_));
            memcpy(_buff + olb_size, string_, len);
            return *this;
        }

        template<typename string_t>
        bfss_string& operator += (const string_t &string_) {
            size_t olb_size = _size;
            resize(_size + string_.size());
            memcpy(_buff + olb_size, string_.c_str(), string_.size());
            return *this;
        }

        bfss_string& operator += (const char *string_) {
            size_t olb_size = _size;
            size_t len = strlen(string_);
            resize(_size + len);
            memcpy(_buff + olb_size, string_, len);
            return *this;
        }

        char & operator[](size_t i_) {
            assert(_size > i_);
            return _buff[i_];
        }

        const char & operator[](size_t i_)const {
            assert(_size > i_);
            return _buff[i_];
        }

        size_t size() const {
            return _size;
        }

        size_t size() {
            return _size;
        }

        size_t length() const {
            return _size;
        }

        size_t length() {
            return _size;
        }

        static const size_t fix_buff_size = _fix_buff_size;

    private:
        friend boost::serialization::access;

        template<class Archive>
        void serialize(Archive & ar, const unsigned int version) {
            ar & _size;
            if (Archive::is_loading::value) {
                size_t size = 0;
                std::swap(size, _size);
                resize(size);
            }
            ar & boost::serialization::make_array(_buff, _size);
        }

        char *_buff;
        char _fix_buff[_fix_buff_size + 1] = "";
        size_t _size = 0;
    };

    template<size_t _fix_buff_size>
    bool operator == (const char *str_,const bfss_string<_fix_buff_size> &str1_) {
        return str1_.operator==(str_);
    }

    template<size_t _fix_buff_size>
    bool operator == (std::string &str_,const bfss_string<_fix_buff_size> &str1_) {
        return str1_.operator==(str_);
    }

    template<size_t _fix_buff_size>
    bool operator != (const char *str_,const bfss_string<_fix_buff_size> &str1_) {
        return str1_.operator!=(str_);
    }

    template<size_t _fix_buff_size>
    bool operator != (std::string &str_,const bfss_string<_fix_buff_size> &str1_) {
        return str1_.operator!=(str_);
    }

    template<size_t _fix_buff_size>
    std::string& operator << (std::string &str_,const bfss_string<_fix_buff_size> &str1_) {
        str_.assign(str1_.c_str(), str1_.size());
        return str_;
    }

    template<typename _manage_t>
    struct object_cache_t {
        int32_t VolumeId = 0;
        int32_t BeginIndex = 0;
        int32_t BeginOffset = 0;
        int32_t Size = 0;
        int64_t Time = 0;
        int32_t Flag = 0;
        bool Complete = false;
        bfss_string<sizeof(oid_blk_info)> ctx;
        bfss_string<oid_max_len> oid;
        bfss_string<object_head_len> head;
        bfss_string<object_hash_hex_len> hash;
        bfss_string<tag_max_len> tag;

        _manage_t &manage;

        object_cache_t() = delete;

        object_cache_t(const object_cache_t &) = delete;

        object_cache_t(const object_cache_t &&) = delete;

        inline object_cache_t(const std::string &_oid, _manage_t &_cache_lock) : manage(_cache_lock), oid(_oid) {
            ctx.resize(sizeof(oid_blk_info));
        }

        template<size_t _fix_buff_size>
        inline
        object_cache_t(bfss_string<_fix_buff_size> &_oid, _manage_t &_cache_lock) : manage(_cache_lock), oid(_oid) {
            ctx.resize(sizeof(oid_blk_info));
        }

        void update() {
            manage.set_cache(oid, *this);
        }

        bool load() {
            return manage.get_cache(oid, *this);
        }

        void del() {
            manage.del_cache(oid);
        }

    private:
        friend std::unique_lock<object_cache_t<_manage_t>>;

        inline void lock() {
            bfss_string<bfss_string<oid_max_len>::fix_buff_size + 7> lock_name = "lock://";
            manage.lock(lock_name + oid);
        }

        inline void unlock() {
            bfss_string<bfss_string<oid_max_len>::fix_buff_size + 7> lock_name = "lock://";
            manage.unlock(lock_name + oid);
        }

        friend boost::serialization::access;

        template<class Archive>
        void serialize(Archive &ar, const unsigned int version) {
            ar & VolumeId;
            ar & BeginOffset;
            ar & BeginIndex;
            ar & Size;
            ar & Time;
            ar & Flag;
            ar & Complete;
            ar & ctx;
            ar & oid;
            ar & head;
            ar & hash;
            ar & tag;
        }
    };

    typedef object_cache_t<bfss_memcache> object_cache;

    template<typename _cache_t>
    inline oid_blk_info *build_cache_context(_cache_t &_cache) {
        oid_blk_info *_fbi = (oid_blk_info *) _cache.ctx.c_str();
        _fbi->BeginIndex = _cache.BeginIndex;
        _fbi->BeginOffset = _cache.BeginOffset;
        _fbi->Size = _cache.Size;
        return _fbi;
    }

    class bfss_streambuf : public  std::streambuf {
    public:
        bfss_streambuf() : _free(false) {}

        explicit bfss_streambuf(std::stringbuf *_buf) : _free(false) {
            _str = _buf->str();
            std::streambuf::setg((char *) _str.c_str(), (char *) _str.c_str(), (char *) _str.c_str() + _str.size());
        }

        inline void setg(const void *_ptr, size_t size) {
            _free = true;
            std::streambuf::setg((char *) _ptr, (char *) _ptr, (char *) _ptr + size);
        }

        ~bfss_streambuf() final {
            if (_free)  free(eback());
        }

        size_t size() const {
            return egptr() - eback();
        }

        char *bptr() const {
            return eback();
        }

        char *eptr() const {
            return egptr();
        }

        char *cptr() const {
            return gptr();
        }

    protected:
        std::string _str;
        bool _free;
    };
}
#endif //BFSS_ALL_BFSS_CACHE_H
