//
// Created by root on 19-2-8.
//

#ifndef BFSS__SN__NODE__H
#define BFSS__SN__NODE__H


#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <queue>

#include <boost/atomic.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <openssl/sha.h>
#include <openssl/evp.h>

#include <libconfig.h++>
#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/helpers/exception.h>

#include <BFSS_SND.h>
#include <BFSS_REGMD.h>
#include <BFSS_APID.h>
#include <bfss_regm_types.h>
#include <bfss_base.h>
#include <bfss_exception.h>
#include "blkdev.h"
#include "bfss_keys.h"
using namespace bfss;

namespace BFSS_SN {

    class cache_node;

    class cache_node {
        enum _node_status {
            NS_UNINITED = 0,
            NS_INITED = 1,
            NS_UPDATED = 2,
            NS_FLUSHED = NS_INITED
        };

        keys_manager &      key_mgr;
        int32_t             cache_index;

        int32_t             blk_index;
        char                blk_data[blk_size];
        boost::shared_mutex blk_mutex;  /* [缓存读写过程]  不能和  [块设备读写过程 或 加解密过程]  相冲突*/

        cache_node *        encrypted;  /*为明文缓存记录密文缓存的指针，密文缓存该指针为nullptr*/
        uint16_t            status;
        int64_t             mtime;

    public:
        cache_node *        prev;
        cache_node *        next;
        explicit cache_node(keys_manager &_key_mgr, int32_t _index) : key_mgr(_key_mgr) {
            cache_index = _index;
            blk_index = -1;
            memset(blk_data, 0, blk_size);
            status = NS_UNINITED;
            mtime = INT64_MAX;
            encrypted = nullptr;
            prev = this;
            next = this;
        };

        int32_t write(const int32_t index, const int32_t offset, const int32_t size, const char *data);
        int32_t read(const int32_t index, const int32_t offset, const int32_t size, std::string &data, bool decrypt);

        bool blk2cache();
        bool cache2blk();
        bool encrypt();
        bool decrypt();

        void reset_index(int32_t index, cache_node *encrypted_node);

        inline void flush() {
            if (is_decrypted()) {
                encrypt();      // 明文cache 只加密
            } else {
                cache2blk();    // 密文cache 则落盘
            }
        }

        inline bool is_decrypted() {
            boost::shared_lock<boost::shared_mutex> read_lock(blk_mutex);
            return (encrypted != nullptr);
        }

        inline bool is_uninited() {
            boost::shared_lock<boost::shared_mutex> read_lock(blk_mutex);
            return (status == NS_UNINITED);
        }

        inline bool is_updated() {
            boost::shared_lock<boost::shared_mutex> read_lock(blk_mutex);
            return (status == NS_UPDATED);
        }

        inline bool is_timeout(int64_t _now, int32_t _timeout) {
            boost::shared_lock<boost::shared_mutex> read_lock(blk_mutex);
            return (mtime + (int64_t) _timeout) < _now;
        }

        inline bool is_me(int32_t index) {
            boost::shared_lock<boost::shared_mutex> read_lock(blk_mutex);
            return (blk_index == index);
        }
        inline int32_t get_blk_index() {
            boost::shared_lock<boost::shared_mutex> read_lock(blk_mutex);
            return blk_index;
        }
        inline std::string to_string() {
            std::ostringstream oss;
            oss << ((encrypted != nullptr) ? " D[" : " E[" ) << cache_index
                << "](" << blk_index << ")" << ((status == NS_UPDATED) ? "U" : " ");
            return oss.str();
        }
    };


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    /*LRU - LeastRecentlyUsed*/
    class lru_indexable_list {
        struct lru_index {
            cache_node *encrypt;
            cache_node *decrypt;
        };

    public:
        lru_indexable_list() {
            _latest = nullptr;
            _cache_index = nullptr;
        }

        ~lru_indexable_list() {
            _latest = nullptr;
            delete [] _cache_index;
            _cache_index = nullptr;
        }

        void init(int32_t _blk_count, int64_t max_cache_size, keys_manager &_key_mgr);

        template<typename _func_t>
        void access(int32_t _index, bool decrypt, _func_t &&_do_action) {
            boost::upgrade_lock<boost::shared_mutex> cache_shared_lock(_cache_mutex);
            cache_node *node = get_node(_index, decrypt, cache_shared_lock);
            _do_action(node);
        }

        template<typename _func_t>
        void traverse(_func_t &&_do_action, bool _direction=false) {
            boost::shared_lock<boost::shared_mutex> list_shared_lock(_list_mutex);
            if (_latest == nullptr) {
                return;
            }

            auto *node = _latest;
            do {
                bool _go_on = _do_action(node);
                if (!_go_on) {
                    return;
                }
                node = (_direction ? node->next :node->prev);
            } while (node != _latest);
        }

    private:
        void update_list(cache_node *node, boost::upgrade_lock<boost::shared_mutex>& list_shared_lock);

        cache_node *get_node(int32_t index, bool decrypt,
                             boost::upgrade_lock<boost::shared_mutex> &cache_shared_lock);

        void get_cache_index(lru_index &_pair, int blk_index, bool decrypt,
                             boost::upgrade_lock<boost::shared_mutex> &cache_shared_lock,
                             boost::upgrade_lock<boost::shared_mutex> &index_shared_lock);

        inline std::string to_string() {
            int x = 5;
            std::string str_next;
            traverse([&](cache_node *node) {
                x--;
                str_next += " -> " + node->to_string();
                return (x > 0);
            }, true);

            x = 5;
            std::string str_prev;
            traverse([&](cache_node *node) {
                x--;
                str_prev = " -> " + node->to_string() + str_prev;
                return (x > 0);
            });

            return str_next + " ... " + str_prev;
        }

        std::shared_ptr<blk_cache_info> _cache_ptr;
        boost::shared_mutex _cache_mutex;

        cache_node *_latest;
        boost::shared_mutex _list_mutex;

        lru_index *_cache_index;
        boost::shared_mutex _index_mutex;
    };
}

#endif //BFSS__SN__NODE__H
