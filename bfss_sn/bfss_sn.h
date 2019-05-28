//
// Created by root on 19-2-8.
//

#ifndef BFSS_ALL_BFSS_SN_H
#define BFSS_ALL_BFSS_SN_H

#include <bsoncxx/json.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/cursor.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/transport/TNonblockingServerTransport.h>
#include <thrift/transport/TNonblockingServerSocket.h>

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
#include "blkdev.h"
#include "bfss_cache.h"
#include <thread>

namespace  BFSS_SN {

    struct hash_node{
        SHA256_CTX  sha256;
        int32_t     hashed;
        hash_node(){
            SHA256_Init(&sha256);
            hashed = 0;
        }
    };

    typedef std::shared_ptr<BFSS_REGM::BFSS_REGMDClient> regmd_client_ptr;


    class BFSS_SNDHandler : virtual public BFSS_SNDIf {

    public:

        BFSS_SNDHandler(const BFSS_SNDHandler &) = delete;
        BFSS_SNDHandler(const std::string &mongodb_server_uri,
                        const std::string &regmd_uri,
                        const std::string &volume_name,
                        int volume_id,
                        int64_t max_block_size,
                        int64_t max_cache_size,
                        const std::string &desc,
                        const std::string &remote_addr);
        ~BFSS_SNDHandler() final;

    private:
        // imp BFSS_SNDIf
        void GetVersion(std::string& _return) final;
        void ManageMessage(MESSAGE_RESULT &_return, const int32_t CmdID, const int32_t Param, const std::string &Data) final;
        ::bfss::BFSS_RESULT::type WriteData(const int32_t index, const int32_t offset, const std::string &data, const std::string &ctx, const int32_t flag) final;
        void ReadData(READ_RESULT &_return, const int32_t index, const int32_t offset, const int32_t size, const int32_t flag) final;
        void CompleteWriteObj(HASH_RESULT &_return, const std::string &ctx) final;
        void GetBlkKey(BLKKEY_RESULT& _return, const int32_t index) final;
        //
    private:

        void update_hash(const int32_t index, const int32_t offset, const std::string &data, const bfss::oid_blk_info *obi);
        void complete_hash(std::string& hash, const bfss::oid_blk_info *obi);
        hash_node* get_node__insert(int64_t hash_key);
        hash_node* get_node__erase(int64_t hash_key);
        BFSS_REGM::BFSS_REGMDClient* get_local_regmd_client();

        std::map<int64_t, hash_node *> _hash_map;
        boost::shared_mutex            _hash_map_mutex;

        cache_manager   _cache_mgr;
        keys_manager    _key_mgr;
        std::string     _regmd_uri;
    };
}
namespace BFSS_SND{
    int StartServer(libconfig::Config &config);
}
#endif //BFSS_ALL_BFSS_SN_H
