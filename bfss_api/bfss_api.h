//
// Created by root on 19-2-8.
//

#ifndef BFSS_ALL_BFSS_API_H
#define BFSS_ALL_BFSS_API_H

#include <boost/thread.hpp>

#include <bsoncxx/json.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
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

#include <libconfig.h++>

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/helpers/exception.h>

#include <BFSS_SND.h>
#include <BFSS_REGMD.h>
#include <BFSS_APID.h>
#include <bfss_regm_types.h>
#include <bfss_base.h>
#include <bfss_mongo_utils.h>
#include <bffs_memcache.h>
#include <bfss_cache.h>

using namespace ::bfss;

namespace BFSS_APID{
    int StartServer(const libconfig::Config &config);
}


namespace BFSS_API {

    typedef std::shared_ptr<BFSS_SN::BFSS_SNDClient> volume_client_ptr;
    typedef std::shared_ptr<BFSS_REGM::BFSS_REGMDClient> regmd_client_ptr;
    typedef std::shared_ptr<bfss_memcache> memcache_client_ptr;

    class BFSS_APIDHandler : virtual public BFSS_APIDIf {

    public:

        BFSS_APIDHandler(const BFSS_APIDHandler &) = delete;
        BFSS_APIDHandler(const std::string &mongo_client_volume_uri,const std::string &regmd_server_uri, const std::string &memcache_uri);

    private:
        // imp BFSS_APIDIf
        void GetVersion(std::string& _return) final;
        BFSS_RESULT::type CreateObject(const std::string& oid, const int32_t size, const int32_t flag, const std::string& tag) final;
        BFSS_RESULT::type DeleteObject(const std::string& oid) final;
        BFSS_RESULT::type Write(const std::string& oid, const int32_t offset, const std::string& data) final;
        BFSS_RESULT::type ResizeObject(const std::string& oid, const int32_t newsize) final;
        BFSS_RESULT::type CompleteObject(const std::string& oid) final;
        void GetObjectInfo(OBJECT_INFO_RESULT& _return, const std::string& oid) final;
        BFSS_RESULT::type ObjectLockHasHash(const std::string& hash, const int32_t size, const std::string& head) final;
        BFSS_RESULT::type CreateObjectLink(const std::string& oid, const std::string& hash, const int32_t size, const std::string& head, const int32_t flag, const std::string& tag) final;
        void Read(READ_RESULT& _return, const std::string& oid, const int32_t size, const int32_t offset) final;
        void ReadBlk(READ_RESULT& _return, const std::string& oid, const int32_t size, const int32_t offset) final;
        void GetObjectBlksInfo(OBJECT_BLKS_RESULT& _return, const std::string& oid) final;
        void GetObjectBlkKey(BLK_KEY_RESULT& _return, const std::string& oid, const int32_t offset) final;
        void ManageMessage(MESSAGE_RESULT& _return, const int32_t CmdId, const int32_t Param, const std::string& Data) final;
        //
    private:
        void _init_db_collection();

        BFSS_SN::BFSS_SNDClient *get_local_sn_client(int VolumeId);
        bfss_memcache *get_local_memcache_client(const std::string &_uri);
        BFSS_REGM::BFSS_REGMDClient* get_local_regmd_client();

        void add_blk_chip(object_cache_t<bfss_memcache> &cache);
        void add_blk_chip(BFSS_REGM::ALLOCATE_BLKS_INFO &blkInfo);
        void read(READ_RESULT &_return, const std::string &oid, const int32_t size,
                  const int32_t offset, const bool decrypt);

        template<typename _HANDLE_T>
        auto operate_dispersed_cache(_HANDLE_T &&on_handle, const bsoncxx::document::view_or_value &&filter) {
            auto oid = get_mongo_value_value_t(filter,"OId",get_utf8().value.to_string());
            object_cache cache(oid,*get_local_memcache_client(_memcache_uri));
            std::unique_lock<object_cache> lock(cache);
            if (!cache.load()) {
                auto doc = execute_mongo_by_object([&filter](auto collection){
                    return collection.find_one(filter);});
                if (!doc) {
                    THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_NOTFOUND, "Incomplete data and information");
                }
                bfss_string<object_head_len * 2> hex_head;
                cache.VolumeId = get_mongo_document_value_t(doc, "VolumeId", get_int32());
                cache.BeginIndex = get_mongo_document_value_t(doc, "BeginIndex", get_int32());
                cache.BeginOffset = get_mongo_document_value_t(doc, "Offset", get_int32());
                cache.Size = get_mongo_document_value_t(doc, "Size", get_int32());
                cache.Time = get_mongo_document_value_t(doc, "CreateTime", get_int64());
                cache.Flag = get_mongo_document_value_t(doc, "Flag", get_int32());
                cache.Complete = get_mongo_document_value_t(doc, "Complete", get_bool());
                cache.tag = get_mongo_document_value_t(doc, "Tag", get_utf8().value.to_string());
                hex_head = get_mongo_document_value_t(doc, "Head", get_utf8().value.to_string());
                cache.hash = get_mongo_document_value_t(doc, "Hash", get_utf8().value.to_string());
                assert(hex_head.size() == std::min(object_head_len,cache.Size) * 2);
                hex2bin(hex_head,cache.head);
                assert(cache.BeginOffset == pos_align(cache.BeginOffset));
                cache.update();
            }
            cache_perception<object_cache> auto_perception(cache);
            return on_handle(cache);
        }

        template<typename _HANDLE_T>
        auto operate_dispersed_cache(_HANDLE_T &&on_handle, const std::string &oid) {
            return operate_dispersed_cache(on_handle,
                                           bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("OId", oid)));
        }

        volume_client_ptr create_volume_client(const std::string &uri);

        template<typename _execute_fun_t>
        auto execute_mongo_by_object(_execute_fun_t _fun){
            auto _entry = _mongo_client_pool.acquire();
            return _fun((*_entry).database("ObjectDb").collection("ObjectCol"));
        }
        template<typename _execute_fun_t>
        auto execute_mongo_by_object_hash(_execute_fun_t _fun){
            auto _entry = _mongo_client_pool.acquire();
            return _fun((*_entry).database("ObjectDb").collection("ObjectHashCol"));
        }
        
        mongocxx::pool _mongo_client_pool;
        const std::string _regmd_server_uri;
        std::map<int, BFSS_REGM::VOLUME_INFO> _volume_infos_map;
        boost::shared_mutex _volume_infos_map_lock;
        std::string _memcache_uri;
    };
}

#endif //BFSS_ALL_BFSS_API_H
