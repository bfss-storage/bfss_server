//
// Created by han on 19-2-27.
//

#ifndef BFSS_ALL_BFFS_MEMCACHE_H
#define BFSS_ALL_BFFS_MEMCACHE_H

#include <string>
#include <streambuf>
#include <sstream>
#include <thread>

#include <boost/serialization/access.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>


#include <bfss_utils.h>
#include <bfss_exception.h>
#include <bfss_cache.h>

namespace bfss {

    inline void _sleep_milliseconds(int64_t _milliseconds){
        std::this_thread::sleep_for(std::chrono::milliseconds(_milliseconds));
    }
    class bfss_memcache {
    public:
        bfss_memcache(const std::string &_uri, const void *_opt = nullptr);
        ~bfss_memcache();

        template<typename _key_t,typename _T>
        bool get_cache(const _key_t &_key,_T &_cache){
            bfss_streambuf _buf;
            if(!_get_cache(_key.c_str(),_key.size(),_buf)){
                return false;
            }
            boost::archive::binary_iarchive(_buf) >> _cache;
            return true;
        }

        template<typename _key_t,typename _T>
        void set_cache(const _key_t &_key,const _T &_cache){
            std::stringstream _buf;
            boost::archive::binary_oarchive(_buf) << _cache;
            bfss_streambuf _tbuf(_buf.rdbuf());
            _set_cache(_key.c_str(),_key.size(),_tbuf);
        }

        template<typename _key_t>
        void del_cache(const _key_t &_key){
            _del_cache(_key.c_str(),_key.size());
        }

        #ifdef DEBUG
            #define _def_time_granularity 100000000
        #else
            #define _def_time_granularity 10000
        #endif

        template<typename _key_t,int64_t _time_granularity = 10>
        void lock(const _key_t & _key,int64_t _outtime = _def_time_granularity,std::function<void(int64_t)>&&_sleep = _sleep_milliseconds){
            lock<_time_granularity>(_key.c_str(),_key.size(),_outtime,std::move(_sleep));
        }
        template<int64_t _time_granularity = 10>
        void lock(const char *_key,size_t _key_len,int64_t _outtime = _def_time_granularity, std::function<void(int64_t)>&&_sleep = _sleep_milliseconds){
            do{
                if(_try_lock(_key, _key_len)){
                    break;
                }
                _sleep(std::min(_time_granularity,_outtime));
                if(_outtime < _time_granularity){
                    THROW_BFSS_EXCEPTION(bfss::BFSS_RESULT::BFSS_TIMEOUT, "call lock out time!");
                }
                _outtime -= _time_granularity;
            }while(true);
        }

        #undef _def_time_granularity
        template<typename _key_t>
        bool try_lock(const _key_t &_key){
            return _try_lock(_key.c_str(),_key.size());
        }
        template<typename _key_t>
        void unlock(const _key_t &_key){
            _unlock(_key.c_str(),_key.size());
        }

    private:

        void _unlock(const char *_key,size_t _key_len);
        bool _try_lock(const char *_key,size_t _key_len);
        bool _del_cache(const char *_key,size_t _key_len);
        void _set_cache(const char *_key,size_t _key_len, const bfss_streambuf &_buf);
        bool _get_cache(const char *_key,size_t _key_len,bfss_streambuf &_buf);

        void _connect();
        void _close();

    private:
        std::string _uri;
        void *_ctx;
        void *_opt;

    };
}
#endif //BFSS_ALL_BFFS_MEMCACHE_H
