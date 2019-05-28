//
// Created by han on 19-2-27.
//

#include "bffs_memcache.h"

#include <libmemcached/memcached.h>
#include <bfss_exception.h>

namespace bfss {
    struct _p_cached_ctx_ {
        memcached_st *cst;
        memcached_server_list_st lst;
    };

#define _check_memcached_result(_x) \
    if (_x != MEMCACHED_SUCCESS){ \
        THROW_BFSS_EXCEPTION(bfss::BFSS_RESULT::BFSS_UNKNOWN_ERROR, memcached_strerror(_cts, _rc));\
    }


#define _cts ((_p_cached_ctx_*)_ctx)->cst
#define _lst ((_p_cached_ctx_*)_ctx)->lst

    bfss_memcache::bfss_memcache(const std::string &_uri,const void* _opt)
    : _uri(_uri),_ctx(nullptr)
    , _opt(const_cast<void*>(_opt)) {
        _connect();
    }
    void bfss_memcache::_connect() {
        auto[_scheme, _host_port] = bfss::parse_uri(_uri, 11211);
        if (_scheme != "memc") {
            THROW_BFSS_EXCEPTION(bfss::BFSS_RESULT::BFSS_SCHEME_ERROR, "cached server scheme error!");
        }
        int _port;
        std::string _host;
        std::tie(_host, _port) = _host_port;

        _close();

        _ctx = (void *) new _p_cached_ctx_();

        _cts = memcached_create((memcached_st *) _opt);
        if (!_cts) {
            THROW_BFSS_EXCEPTION(bfss::BFSS_RESULT::BFSS_NO_MEMORY, "call memcached create error!");
        }
        memcached_return_t _rc;
        _lst = memcached_server_list_append(nullptr, _host.c_str(), _port, &_rc);
        _check_memcached_result(_rc);
        _check_memcached_result(memcached_server_push(_cts,_lst));
        _check_memcached_result(memcached_server_push(_cts,_lst));
        _check_memcached_result(memcached_behavior_set(_cts,MEMCACHED_BEHAVIOR_SND_TIMEOUT,20));
        _check_memcached_result(memcached_behavior_set(_cts,MEMCACHED_BEHAVIOR_RCV_TIMEOUT,20));
        _check_memcached_result(memcached_behavior_set(_cts,MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT,1000));
    }
    void bfss_memcache::_close() {
        if(_ctx){
            if(_cts) memcached_free(_cts);
            if(_lst) memcached_server_list_free(_lst);
            delete ((_p_cached_ctx_*)_ctx);
        }
    }
    bfss_memcache::~bfss_memcache(){
        _close();
    }


    bool bfss_memcache::_get_cache(const char *_key,size_t _key_len,bfss_streambuf &_buf){
        size_t size;
        uint32_t flags;
        memcached_return_t _rc;
        while(true){
            char *_ptr = memcached_get(_cts,_key,_key_len,&size,&flags,&_rc);
            if(_rc == MEMCACHED_NOTFOUND || (nullptr == _ptr && _rc == MEMCACHED_SUCCESS)){
                return false;
            }
            if(_rc == MEMCACHED_CONNECTION_FAILURE || _rc == MEMCACHED_TIMEOUT){
                _connect();
                continue;
            }
            if (_rc != MEMCACHED_SUCCESS) {
                THROW_BFSS_EXCEPTION(bfss::BFSS_RESULT::BFSS_UNKNOWN_ERROR, memcached_strerror(_cts, _rc));
            }
            _buf.setg(_ptr,size);
            return true;
        }
    }

    void bfss_memcache::_set_cache(const char *_key,size_t _key_len, const bfss_streambuf &_buf){
        while(true) {
            memcached_return_t _rc = memcached_set(_cts, _key, _key_len, _buf.bptr(), _buf.size(), 60, 0);
            if (_rc != MEMCACHED_SUCCESS) {
                if(_rc == MEMCACHED_CONNECTION_FAILURE || _rc == MEMCACHED_TIMEOUT){
                    _connect();
                    continue;
                }
                THROW_BFSS_EXCEPTION(bfss::BFSS_RESULT::BFSS_UNKNOWN_ERROR, memcached_strerror(_cts, _rc));
            }
            return;
        }
    }

    bool bfss_memcache::_try_lock(const char *_key,size_t _key_len){
        while(true) {
            memcached_return_t _rc = memcached_add(_cts, _key, _key_len, "", 0, 20, 0);
            if(_rc == MEMCACHED_CONNECTION_FAILURE || _rc == MEMCACHED_TIMEOUT){
                _connect();
                continue;
            }
            if (_rc != MEMCACHED_NOTSTORED && _rc != MEMCACHED_SUCCESS) {
                THROW_BFSS_EXCEPTION(bfss::BFSS_RESULT::BFSS_UNKNOWN_ERROR, memcached_strerror(_cts, _rc));
            }
            return _rc == MEMCACHED_SUCCESS;
        }
    }

    void bfss_memcache::_unlock(const char *_key,size_t _key_len) {
        while (true) {
            memcached_return_t _rc = memcached_delete(_cts, _key, _key_len, 0);
            if(_rc == MEMCACHED_CONNECTION_FAILURE || _rc == MEMCACHED_TIMEOUT){
                _connect();
                continue;
            }
            if (_rc != MEMCACHED_NOTFOUND && _rc != MEMCACHED_SUCCESS) {
                THROW_BFSS_EXCEPTION(bfss::BFSS_RESULT::BFSS_UNKNOWN_ERROR, memcached_strerror(_cts, _rc));
            }
            return;
        }
    }

    bool bfss_memcache::_del_cache(const char *_key,size_t _key_len){
        while(true) {
            memcached_return_t _rc = memcached_delete(_cts, _key, _key_len, 0);
            if(_rc == MEMCACHED_CONNECTION_FAILURE || _rc == MEMCACHED_TIMEOUT){
                _connect();
                continue;
            }
            if (_rc != MEMCACHED_SUCCESS && _rc != MEMCACHED_NOTFOUND) {
                THROW_BFSS_EXCEPTION(bfss::BFSS_RESULT::BFSS_UNKNOWN_ERROR, memcached_strerror(_cts, _rc));
            }
            return _rc == MEMCACHED_SUCCESS;
        }
    }
}