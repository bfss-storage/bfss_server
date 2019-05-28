//
// Created by root on 19-2-8.
//

#ifndef BFSS_ALL_BFSS_REGM_H
#define BFSS_ALL_BFSS_REGM_H

#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <iostream>
#include <queue>
#include <thread>

#include <boost/thread/thread.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/atomic.hpp>

#include <openssl/sha.h>
#include <openssl/evp.h>

#include <libconfig.h++>

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>

#include <thrift/protocol/TBinaryProtocol.h>

#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TNonblockingServer.h>

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TNonblockingServerTransport.h>
#include <thrift/transport/TNonblockingServerSocket.h>
#include <thrift/concurrency/PosixThreadFactory.h>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/cursor.hpp>

#include <BFSS_REGMD.h>
#include <bfss_base.h>
#include <bfss_mongo_utils.h>

namespace BFSS_REGM {

    using namespace ::bfss;

    class BFSS_REGMDHandler : virtual public BFSS_REGMDIf {
    public:
        explicit BFSS_REGMDHandler(const std::string &volume_mongo_uri);
        ~BFSS_REGMDHandler();

        BFSS_REGMDHandler(const BFSS_REGMDHandler &) = delete;

        void GetVersion(std::string& _return) final;
        void ManageMessage(MESSAGE_REGM_RESULT &_return, const int32_t CmdId, const int32_t Param, const std::string &Data) final;
        BFSS_RESULT::type RegisterVolume(const REGISTER_VOLUME_INFO &VolumeInfo) final;
        void GetVolumeInfo(VOLUME_RESULT &_return, const int32_t VolumeId) final;
        void GetAllVolumeInfo(std::vector<VOLUME_INFO> &_return) final;
        BFSS_RESULT::type AddBlkChip(const ALLOCATE_BLKS_INFO &blksInfo) final;
        void AllocateBlks(ALLOCATE_BLKS_RESULT &_return, const int32_t size) final;

    private:
        template<typename _execute_fun_t>
        auto execute_mongo_by_volumecol(_execute_fun_t _fun){
            auto _entry = _mongo_client_pool.acquire();
            return _fun((*_entry).database("VolumeDb").collection("VolumeCol"));
        }

        template<typename _execute_fun_t>
        auto execute_mongo_by_volumechipcol(_execute_fun_t _fun) {
            auto _entry = _mongo_client_pool.acquire();
            return _fun((*_entry).database("VolumeDb").collection("VolumeChipCol"));
        }

        void _init_db_collection();

        void _rebuild_sn_queue();
        void _sn_find();

        size_t      _get_volume_count();
        int32_t     _get_volume_id();


        mongocxx::pool _mongo_client_pool;

        std::queue<int32_t> _sn_client_queue;
        std::mutex          _sn_client_rwlock;

        std::shared_ptr<std::thread>    _sn_find_thread;
        std::condition_variable         _condition;
        std::mutex                      _condition_mutex;
    };
}
namespace BFSS_REGMD {
    int StartServer(const libconfig::Config &config);
}
#endif //BFSS_ALL_BFSS_REGM_H
