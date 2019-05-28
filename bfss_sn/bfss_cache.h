//
// Created by root on 19-2-8.
//

#ifndef BFSS__SN__CACHE__H
#define BFSS__SN__CACHE__H


#include <bsoncxx/json.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/cursor.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/pool.hpp>

#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <queue>
#include <thread>

#include <boost/atomic.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lockfree/spsc_queue.hpp>


#include <libconfig.h++>
#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/helpers/exception.h>

#include <BFSS_SND.h>
#include <BFSS_REGMD.h>
#include <BFSS_APID.h>
#include <bfss_regm_types.h>
#include <bfss_base.h>
#include <bfss_utils.h>
#include "bfss_node.h"
#include "blkdev.h"

namespace  BFSS_SN {

    class cache_manager {
    public:
        cache_manager():_initd(false) {

        }
        ~cache_manager() {
            flush_and_stop();
        }

        int32_t init(const std::string &volume_name, int64_t max_block_size, int64_t max_cache_size,
                     keys_manager &blkey_mgr);

        int32_t write(const int32_t index, const int32_t offset, const int32_t size, const char *data);
        int32_t read(const int32_t index, const int32_t offset, const int32_t size, std::string &data, bool decrypt);

        void flush_and_stop();

    private:
        boost::lockfree::spsc_queue<int, boost::lockfree::capacity<1024> >  _spsc_queue;

        lru_indexable_list              _lru_list;

        std::condition_variable         _condition;
        std::mutex                      _condition_mutex;
        volatile bool                   _initd;

        std::shared_ptr<std::thread>    _cache_timeout_thread;
        std::shared_ptr<std::thread>    _cache___async_thread;

        void flush_async();
        void flush_timeout();
    };

}


#endif //BFSS__SN__CACHE__H
