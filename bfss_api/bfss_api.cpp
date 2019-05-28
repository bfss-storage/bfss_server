//
// Created by root on 19-2-8.
//
#include "bfss_api.h"
#include <bfss_signal.h>
#include <bfss_cilent_helper.h>

using namespace ::BFSS_API;

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::stdcxx;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::concurrency;

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

using bsoncxx::document::view;
using bsoncxx::document::element;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::finalize;

using mongocxx::collection;

using namespace ::mongocxx::model;


namespace BFSS_API {

    thread_local std::map<int, volume_client_ptr> _volume_clients_map;
    thread_local regmd_client_ptr _regmd_client(nullptr);
    thread_local memcache_client_ptr _memcache_client(nullptr);

    BFSS_REGM::BFSS_REGMDClient* BFSS_APIDHandler::get_local_regmd_client() {
        if ( !_regmd_client ) {
            auto [rgmd_scheme,_rgmd_host_port] = bfss::parse_uri(_regmd_server_uri,9091);
            if(rgmd_scheme != "regmd"){
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_SCHEME_ERROR,"regmd server scheme error!");
            }
            int _rgmd_port;
            std::string _rgmd_host;
            std::tie(_rgmd_host,_rgmd_port) = _rgmd_host_port;
            shared_ptr<TTransport> socket(new TSocket(_rgmd_host,_rgmd_port));
            shared_ptr<TTransport> transport(new TFramedTransport(socket));
            shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
            _regmd_client = regmd_client_ptr(new bfss_cilent_helper<BFSS_REGM::BFSS_REGMDClient>(protocol));
        }
        return static_cast<bfss_cilent_helper<BFSS_REGM::BFSS_REGMDClient>*>(_regmd_client.get())->get_client();
    }

    bfss::bfss_memcache *BFSS_APIDHandler::get_local_memcache_client(const std::string &_uri){
        if ( !_memcache_client ) {
            _memcache_client = memcache_client_ptr(new bfss_memcache(_uri));
        }
        return _memcache_client.get();
    }

    volume_client_ptr BFSS_APIDHandler::create_volume_client(const std::string &_uri){

        auto [snd_scheme,_snd_host_port] = bfss::parse_uri(_uri,9092);
        if(snd_scheme != "snd"){
            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_SCHEME_ERROR,"snd server scheme error!");
        }
        int _snd_port;
        std::string _snd_host;
        std::tie(_snd_host,_snd_port) = _snd_host_port;

        stdcxx::shared_ptr <TTransport> socket(new TSocket(_snd_host, _snd_port));
        stdcxx::shared_ptr <TTransport> transport(new TFramedTransport(socket));
        stdcxx::shared_ptr <TProtocol> protocol(new TBinaryProtocol(transport));

        return volume_client_ptr(new bfss_cilent_helper<BFSS_SN::BFSS_SNDClient>(protocol));
    }

    BFSS_APIDHandler::BFSS_APIDHandler(const std::string &mongo_client_volume_uri,const std::string &regmd_server_uri, const std::string &memcache_uri)
    : _mongo_client_pool(mongocxx::uri(mongo_client_volume_uri)), _regmd_server_uri(regmd_server_uri),_memcache_uri(memcache_uri) {
        _init_db_collection();
    }

    void BFSS_APIDHandler::_init_db_collection() {
        mongocxx::options::index index_options{};
        index_options.unique(true);
        execute_mongo_by_object([&index_options](auto collection){collection.create_index(make_document(kvp("OId", 1)), index_options);});
        execute_mongo_by_object([](auto collection){collection.create_index(make_document(kvp("OId", 1),kvp("Complete", 1)));});
        execute_mongo_by_object([](auto collection){collection.create_index(make_document(kvp("Hash", 1), kvp("Size", 1) ,kvp("Head", 1), kvp("Complete", 1)));});
        execute_mongo_by_object_hash([](auto collection){collection.create_index(make_document(kvp("Hash", 1), kvp("Size", 1), kvp("Head", 1)));});
    }

    void BFSS_APIDHandler::GetVersion(std::string& _return) {
        /* system.component.major.minor.build.private */
        _return = std::string("bfss.api.0.0.1.alpha");
    }


    BFSS_RESULT::type BFSS_APIDHandler::CreateObject(const std::string &oid, const int32_t size, const int32_t flag,
            const std::string &tag) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, oid, size, flag, tag);
        try {
            if (!check_oid(oid) || size <= 0 || size > object_max_size || flag & 0xffff0000 || tag.length() > tag_max_len) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR,"param error!");
            }
            execute_mongo_by_object([&oid](auto collection){collection.insert_one(make_document(kvp("OId", oid)));});
            BFSS_REGM::ALLOCATE_BLKS_RESULT blks_result;
            bfss_try([&]{
                //get object area size  allocate data_align 16 BYTE
                try_call_t(get_local_regmd_client(),
                           &BFSS_REGM::BFSS_REGMDClient::AllocateBlks,  blks_result, size);
                if (BFSS_RESULT::BFSS_SUCCESS != blks_result.Result) {
                    execute_mongo_by_object([&oid](auto collection){collection.delete_one(make_document(kvp("OId", oid)));});
                    THROW_BFSS_EXCEPTION(blks_result.Result, "AllocateBlks failed!");
                }
                LOG4CXX_INFO(logger, "AllocateBlks:" << blks_result);
            },[&](BFSS_RESULT::type type, const char *what) {
                execute_mongo_by_object([&oid](auto collection){collection.delete_one(make_document(kvp("OId", oid)));});
                THROW_BFSS_EXCEPTION(type, what);
            });
            //断言，必要判断，申请空间是否正确
            assert(get_oid_size(blks_result.BlksInfo.BeginIndex, blks_result.BlksInfo.BeginOffset,
                    blks_result.BlksInfo.EndIndex, blks_result.BlksInfo.EndOffset) == data_align(size) &&
                    0 != blks_result.BlksInfo.VolumeId );
            std::string hex;
            size_t head_size = (size_t)std::min(object_head_len, size);
            hex.resize(head_size * 2, '0');
            auto cTime = (int64_t)get_time_stamp();
            auto doc = execute_mongo_by_object([&](auto collection){
                return collection.find_one_and_update(
                    make_document(kvp("OId", oid)),
                    make_document(kvp("$set", make_document(
                            kvp("VolumeId",   blks_result.BlksInfo.VolumeId),
                            kvp("BeginIndex", blks_result.BlksInfo.BeginIndex),
                            kvp("Offset",     blks_result.BlksInfo.BeginOffset),
                            kvp("Size", size),
                            kvp("CreateTime", cTime),
                            kvp("Flag", flag),
                            kvp("Tag", tag),
                            kvp("Hash", ""),
                            kvp("Head", hex),
                            kvp("Complete", false)
                            ))));});
            assert(doc);
            // for lock cache
            {
                object_cache cache(oid,*get_local_memcache_client(_memcache_uri));
                std::unique_lock<object_cache> lock(cache);

                cache.VolumeId = blks_result.BlksInfo.VolumeId;
                cache.BeginIndex = blks_result.BlksInfo.BeginIndex;
                cache.BeginOffset = blks_result.BlksInfo.BeginOffset;
                cache.Size = size;
                cache.Complete = false;
                cache.Time = cTime;
                cache.tag = tag;
                cache.head.resize(head_size);
                cache.update();
            }
            int volumeid = blks_result.BlksInfo.VolumeId;

            BFSS_REGM::VOLUME_RESULT result;
            bfss_try([&]{
                try_call_t(get_local_regmd_client(),
                           &BFSS_REGM::BFSS_REGMDClient::GetVolumeInfo,  result, volumeid);
                if (result.Result != BFSS_RESULT::BFSS_SUCCESS) {
                    THROW_BFSS_EXCEPTION(result.Result, "GetVolumeInfo failed!");
                }
            },[&](BFSS_RESULT::type type, const char *what) {
                execute_mongo_by_object(
                        [&oid](auto collection) { collection.delete_one(make_document(kvp("OId", oid))); });
                try_call_t(get_local_regmd_client(),
                           &BFSS_REGM::BFSS_REGMDClient::AddBlkChip, blks_result.BlksInfo);
                THROW_BFSS_EXCEPTION(type, what);
            });

            // for lock volume_infos_map
            {
                std::lock_guard _lock(_volume_infos_map_lock);
                auto it = _volume_infos_map.find(volumeid);
                if (it == _volume_infos_map.end()) {
                    _volume_infos_map[volumeid] = result.Info;
                }
            }
            FUNCTION_EXIT_DEBUG_LOG(logger, oid, size, flag, tag);
            return BFSS_RESULT::BFSS_SUCCESS;
        } CATCH_BFSS_EXCEPTION_RETURN();
    }

    BFSS_SN::BFSS_SNDClient *BFSS_APIDHandler::get_local_sn_client(int VolumeId) {
        // for lock volume_infos_map
        boost::upgrade_lock read_lock(_volume_infos_map_lock);
        if (!has_lockup(_volume_infos_map, VolumeId)) {
            boost::upgrade_to_unique_lock writ_lock(read_lock);
            if(!has_lockup(_volume_infos_map, VolumeId)){
                BFSS_REGM::VOLUME_RESULT volume_result;
                try_call_t(get_local_regmd_client(),
                           &BFSS_REGM::BFSS_REGMDClient::GetVolumeInfo,  volume_result, VolumeId);
                if (volume_result.Result != BFSS_RESULT::BFSS_SUCCESS) {
                    THROW_BFSS_EXCEPTION(volume_result.Result, "SNDClient not found.");
                }
                _volume_infos_map[VolumeId] = volume_result.Info;
            }
        }

        volume_client_ptr new_client;
        std::map<int, volume_client_ptr>::iterator iter;
        if (!has_lockup(_volume_clients_map,VolumeId, iter)){
            bfss_try([&](){
                boost::upgrade_to_unique_lock writ_lock(read_lock);
                if(!has_lockup(_volume_clients_map,VolumeId, iter)) {
                    new_client = create_volume_client(_volume_infos_map[VolumeId].Uri);
                    _volume_clients_map[VolumeId] = new_client;
                    FUNCTION_DEBUG_LOG(logger, "create_volume_client:" << VolumeId << "  - "
                                                                       << _volume_infos_map[VolumeId].Uri);
                }
            },[&](BFSS_RESULT::type type, const char *what) {
                boost::upgrade_to_unique_lock writ_lock(read_lock);
                _volume_infos_map.erase(VolumeId);
                FUNCTION_DEBUG_LOG(logger, "delete_volume_client:" << VolumeId);
                THROW_BFSS_EXCEPTION(type, what);
            });
        } else {
            new_client = iter->second;
        }
        return static_cast<bfss_cilent_helper<BFSS_SN::BFSS_SNDClient>*>(new_client.get())->get_client();
    }

    //offset 文件偏移
    BFSS_RESULT::type BFSS_APIDHandler::Write(const std::string &oid, const int32_t offset, const std::string &data) {

        FUNCTION_ENTRY_DEBUG_LOG(logger, oid, offset, data.length());

        try {
            if (!check_oid(oid) || offset < 0 || data.length() > chip_max_size) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "param error!");
            }
            operate_dispersed_cache(
                    [&](object_cache &cache){
                        if (cache.Complete) {
                            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_COMPLETED, "The object have been Completed.");
                        }
                        if (offset >= cache.Size) {
                            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "Write object offset > size.");
                        }

                        build_cache_context(cache);

                        LOG4CXX_DEBUG(logger,
                                      "-Write(cache.Size:" << cache.Size << "," <<
                                                           "cache.Complete:" << cache.Complete << "," <<
                                                           "cache.BeginIndex:" << cache.BeginIndex << "," <<
                                                           "cache.BeginOffset:" << cache.BeginOffset << ")-");
                        if(data.length() == 0){
                            return;
                        }

                        int blkOffset = get_blk_begin_offset(
                                get_oid_begin_pos(cache.BeginIndex, cache.BeginOffset) + offset,
                                false);

                        int blkIndex = get_blk_index(cache.BeginIndex, cache.BeginOffset, pos_align(offset));

                        auto data_size = std::min(size_t(cache.Size - offset), data.length());
                        const std::string & _data = data_size >= data.length() ? data : data.substr(0, data_size);

                        int head_len = std::min(object_head_len, cache.Size);
                        if (head_len > offset) {
                            cache.head.resize((size_t)head_len);
                            auto app_size = std::min((size_t)head_len - offset, _data.length());
                            memcpy(const_cast<char*>(cache.head.c_str()) + (size_t)offset, _data.c_str(), app_size);
                            std::string hex;
                            execute_mongo_by_object([&](auto collection) {
                                return collection.find_one_and_update(
                                        make_document(kvp("OId", oid)),
                                        make_document(kvp("$set", make_document(kvp("Head", bin2hex(cache.head, hex))))));
                            });
                        }
                        int _flags = 0;
                        auto ctx = cache.ctx.to_string();
                        BFSS_RESULT::type result = try_call_t(get_local_sn_client(cache.VolumeId),
                                                              &BFSS_SN::BFSS_SNDClient::WriteData, blkIndex, blkOffset, _data, ctx, _flags);
                        if (BFSS_RESULT::BFSS_SUCCESS != result) {
                            THROW_BFSS_EXCEPTION(result, "sndClient::WriteData failed! oid=" + oid);
                        }

                        if ((offset + data.length()) > cache.Size) {
                            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_DATA_WRITE_INCOMPLETE,
                                                 "sndClient::WriteData incomplete! oid=" + oid);
                        }
                    },
                    oid);
            FUNCTION_EXIT_DEBUG_LOG(logger, oid, offset);
            return BFSS_RESULT::BFSS_SUCCESS;
        } CATCH_BFSS_EXCEPTION_RETURN();
    }

    BFSS_RESULT::type BFSS_APIDHandler::CompleteObject(const std::string &oid) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, oid);

        try {
            if (!check_oid(oid)) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR,"param error!");
            }
            operate_dispersed_cache(
                    [&](object_cache &cache)  {

                        if (cache.Complete) {
                            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_COMPLETED,
                                                 "The object have been Completed. oid=" + oid);
                        }

                        build_cache_context(cache);

                        BFSS_SN::HASH_RESULT result;
                        auto ctx = cache.ctx.to_string();
                        try_call_t(get_local_sn_client(cache.VolumeId),
                                   &BFSS_SN::BFSS_SNDClient::CompleteWriteObj, result, ctx);
                        if (result.Result != BFSS_RESULT::BFSS_SUCCESS) {
                            THROW_BFSS_EXCEPTION(result.Result, "SNDClient::CompleteWriteObj failed! oid=" + oid);
                        }

                        assert(std::min(object_head_len, cache.Size) == cache.head.length());

                        std::string hash_hex;
                        bin2hex(result.hash,hash_hex);
                        auto object_doc = execute_mongo_by_object([&](auto collection) {
                            return collection.find_one_and_update(
                                    make_document(kvp("OId", oid)),
                                    make_document(kvp("$set", make_document(
                                            kvp("Hash", hash_hex),
                                            kvp("Complete", true)))));
                        });
                        if (object_doc) {

                            cache.Complete = true;
                            cache.hash = hash_hex;

                            mongocxx::options::find_one_and_update find_options{};
                            find_options.return_document(mongocxx::options::return_document::k_after);
                            find_options.upsert(true);
                            auto hash_doc = execute_mongo_by_object_hash([&](auto collection) {
                                return collection.find_one_and_update(
                                        make_document(
                                                kvp("Hash", hash_hex),
                                                kvp("Size", cache.Size),
                                                kvp("Head", get_mongo_document_value_t(object_doc, "Head", get_utf8().value.to_string()))),
                                        make_document(
                                                kvp("$inc", make_document(kvp("RefCount", 1))),
                                                kvp("$inc", make_document(kvp("_RefCount", 1))),
                                                kvp("$addToSet", make_document(kvp("OIdArray", oid))),
                                                kvp("$setOnInsert",
                                                    make_document(
                                                            kvp("VolumeId", cache.VolumeId),
                                                            kvp("BeginIndex", cache.BeginIndex),
                                                            kvp("Offset", cache.BeginOffset)))),
                                        find_options);
                            });

                            /* instead of RefCount, "_RefCount" is a increase only field.. */
                            int refCount = get_mongo_document_value_t(hash_doc, "_RefCount", get_int32());
                            if (refCount > 1) {
                                add_blk_chip(cache);
                                LOG4CXX_DEBUG(logger, "object duplicated, reset : " << cache.BeginIndex << "-"
                                                                                    << cache.BeginOffset);
                                cache.VolumeId = get_mongo_document_value_t(hash_doc, "VolumeId", get_int32());
                                cache.BeginIndex = get_mongo_document_value_t(hash_doc, "BeginIndex", get_int32());
                                cache.BeginOffset = get_mongo_document_value_t(hash_doc, "Offset", get_int32());
                                LOG4CXX_DEBUG(logger, "object duplicated, to    : " << cache.BeginIndex << "-"
                                                                                    << cache.BeginOffset);
                            }

                            execute_mongo_by_object([&](auto collection) {
                                return collection.find_one_and_update(
                                        make_document(
                                                kvp("_id", get_mongo_document_value_t(object_doc, "_id", get_oid()))),
                                        make_document(kvp("$set",
                                                          make_document(
                                                                  kvp("VolumeId", cache.VolumeId),
                                                                  kvp("BeginIndex", cache.BeginIndex),
                                                                  kvp("Offset", cache.BeginOffset)))));
                            });
                        } else {
                            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_NOTFOUND, "find_one_and_update failed! oid=" + oid);
                        }
                    },
                    oid);
            FUNCTION_EXIT_DEBUG_LOG(logger, oid);
            return BFSS_RESULT::BFSS_SUCCESS;
        } CATCH_BFSS_EXCEPTION_RETURN();
    }


    void BFSS_APIDHandler::add_blk_chip(object_cache &cache)
    {
        BFSS_REGM::ALLOCATE_BLKS_INFO blkInfo;

        blkInfo.VolumeId    = cache.VolumeId;
        blkInfo.BeginIndex  = cache.BeginIndex;
        blkInfo.BeginOffset = cache.BeginOffset;
        blkInfo.EndIndex    = get_blk_index(
                cache.BeginIndex, cache.BeginOffset, data_align(cache.Size));
        blkInfo.EndOffset   = get_blk_end_offset(
                cache.BeginIndex, cache.BeginOffset, data_align(cache.Size));
        LOG4CXX_DEBUG(logger, "AddBlkChip BeginIndex:" << blkInfo.BeginIndex << ", BeginOffset:" << blkInfo.BeginOffset );
        add_blk_chip(blkInfo);
    }
    void BFSS_APIDHandler::add_blk_chip(BFSS_REGM::ALLOCATE_BLKS_INFO &blkInfo)
    {
        auto result = try_call_t(get_local_regmd_client(),
                           &BFSS_REGM::BFSS_REGMDClient::AddBlkChip, blkInfo);
        if(result != BFSS_RESULT::BFSS_SUCCESS) {
            THROW_BFSS_EXCEPTION(result, "call SNDClient::AddBlkChip failed!");
        }
    }

    BFSS_RESULT::type BFSS_APIDHandler::DeleteObject(const std::string &oid) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, oid);

        try {
            if (!check_oid(oid)) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "param error!");
            }
            operate_dispersed_cache(
                    [&](object_cache &cache)  {
                        if (!cache.Complete) {
                            add_blk_chip(cache);
                            return;
                        }
                        execute_mongo_by_object([&](auto collection) {
                            return collection.delete_one(make_document(kvp("OId", oid)));
                        });

                        execute_mongo_by_object_hash([&](auto collection) {
                            std::string hex;
                            return collection.update_one(
                                    make_document(
                                            kvp("Hash", cache.hash.to_string()),
                                            kvp("Size", cache.Size),
                                            kvp("Head", bin2hex(cache.head, hex))),
                                    make_document(
                                            kvp("$inc", make_document(kvp("RefCount", -1))),
                                            kvp("$pull", make_document(kvp("OIdArray", oid)))));
                        });
                        cache.del();
                    },
                    oid);
            FUNCTION_EXIT_DEBUG_LOG(logger, oid);
            return BFSS_RESULT::BFSS_SUCCESS;
        } CATCH_BFSS_EXCEPTION_RETURN();
    }

    void BFSS_APIDHandler::GetObjectInfo(OBJECT_INFO_RESULT &_return, const std::string &oid) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, oid);
        try {
            if (!check_oid(oid)) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "param error!");
            }
            operate_dispersed_cache(
                    [&](object_cache &cache) {
                        OBJECT_INFO objectInfo;

                        objectInfo.ObjectSize = cache.Size;
                        objectInfo.CreateTime = cache.Time;
                        objectInfo.ObjectFlag = cache.Flag & 0x0000ffff;    /* high bits is for internal usage.*/
                        objectInfo.Complete   = cache.Complete;
                        objectInfo.ObjectTag  << cache.tag;
                        objectInfo.Hash       << cache.hash;

                        _return.__set_ObjectInfo(objectInfo);
                        _return.Result = BFSS_RESULT::BFSS_SUCCESS;
                        FUNCTION_EXIT_DEBUG_LOG(logger, oid);;
                    },
                    make_document(kvp("OId", oid)));
        } CATCH_BFSS_EXCEPTION_RESULT(_return);
    }

    BFSS_RESULT::type BFSS_APIDHandler::ObjectLockHasHash(const std::string &hash,
            const int32_t size, const std::string &head) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, hash, size, head);
        try {
            if (size < 0 || size > object_max_size || head.size() > object_head_len) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "param error!");
            }
            if(!execute_mongo_by_object_hash([&](auto collection){
                        std::string hex;
                        return collection.find_one(make_document(
                                kvp("Hash", hash),
                                kvp("Size", size),
                                kvp("Head", bfss::bin2hex(head, hex))));})) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_NOTFOUND, " find one none failed!");
            }
            FUNCTION_EXIT_DEBUG_LOG(logger, hash, size, head);
            return BFSS_RESULT::BFSS_SUCCESS;
        } CATCH_BFSS_EXCEPTION_RETURN();
    }

    void BFSS_APIDHandler::read(READ_RESULT &_return, const std::string &oid, const int32_t size,
                                const int32_t offset, const bool decrypt) {
        try {
            if (!check_oid(oid) || offset < 0 || size > chip_max_size || size < 0) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR,"param error!");
            }
            if ((!decrypt) && (size % blk_align_size || offset % blk_align_size) ) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "param error!");
            }
            operate_dispersed_cache(
                [&](object_cache &cache)  {
                    if (offset >= cache.Size) {
                        THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "Read  object offset > size.");
                    }
                    if (!cache.Complete) {
                        THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_INCOMPLETED, " cache.Complete = false.");
                    }
                    assert(decrypt || (data_align(size) == size && pos_align(offset) == offset));
                    auto data_size = 0;
                    if (decrypt) {
                        data_size = std::min(cache.Size - offset, size);
                    } else {
                        // 读取密钥数据需要数据对齐
                        data_size = std::min(data_align(cache.Size) - offset, data_align(size));
                    }

                    int blkOffset = get_blk_begin_offset(
                            get_oid_begin_pos(cache.BeginIndex,cache.BeginOffset) + offset,false);

                    int blkIndex = get_blk_index(cache.BeginIndex,cache.BeginOffset,pos_align(offset));

                    BFSS_SN::READ_RESULT _data;
                    try_call_t(get_local_sn_client(cache.VolumeId),
                            &BFSS_SN::BFSS_SNDClient::ReadData,  _data, blkIndex, blkOffset, data_size, decrypt);
                    if (_data.Result != BFSS_RESULT::BFSS_SUCCESS){
                        THROW_BFSS_EXCEPTION(_data.Result, "sndClient->ReadData failed.");
                    }

                    _return.Result = BFSS_RESULT::BFSS_SUCCESS;
                    _return.__set_Data(_data.Data);
                    if ((offset + size) > (decrypt ? cache.Size: data_align(cache.Size))) {
                        THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_DATA_READ_INCOMPLETE, " Read incomplete.");
                    }
                },
                make_document(kvp("OId", oid), kvp("Complete", true)));
            FUNCTION_EXIT_DEBUG_LOG(logger, oid, size, offset);
        } CATCH_BFSS_EXCEPTION_RESULT(_return);
    }

    void BFSS_APIDHandler::Read(READ_RESULT &_return, const std::string &oid, const int32_t size, const int32_t offset) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, oid, size, offset);

        read(_return, oid, size, offset, true);

    }
    void BFSS_APIDHandler::ReadBlk(READ_RESULT &_return, const std::string &oid, const int32_t size, const int32_t offset) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, oid, size, offset);

        read(_return, oid, size, offset, false);
    }
    void BFSS_APIDHandler::GetObjectBlksInfo(OBJECT_BLKS_RESULT &_return, const std::string &oid) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, oid);
        try {
            if (!check_oid(oid)) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "param error!");
            }
            operate_dispersed_cache(
                [&](object_cache &cache) {
                    OBJECT_BLKS_INFO object_blk_info;
                    object_blk_info.ObjectInfo.ObjectSize = cache.Size;
                    object_blk_info.ObjectInfo.CreateTime = cache.Time;
                    object_blk_info.ObjectInfo.ObjectTag << cache.tag;

                    std::vector<OBJECT_BLK_INFO> object_blk_lst;

                    int32_t _size = data_align(cache.Size);
                    int32_t _index = cache.BeginIndex;
                    int32_t _offset = cache.BeginOffset;
                    int32_t _data_offset = 0;
                    while (_size > 0) {

                        BFSS_SN::BLKKEY_RESULT blkkey_result;
                        try_call_t(get_local_sn_client(cache.VolumeId),
                                &BFSS_SN::BFSS_SNDClient::GetBlkKey,  blkkey_result, _index);
                        if(blkkey_result.Result != BFSS_RESULT::BFSS_SUCCESS){
                            THROW_BFSS_EXCEPTION(static_cast<BFSS_RESULT::type >(blkkey_result.Result), "SNDClient not found.");
                        }
                        OBJECT_BLK_INFO blk_info;
                        blk_info.BlkSize = std::min((blk_size - _offset), _size);
                        blk_info.DataOff = _data_offset;
                        blk_info.VolumeId = cache.VolumeId;
                        blk_info.BlkIndex = _index;
                        blk_info.BlkKey = blkkey_result.Data;
                        object_blk_lst.push_back(blk_info);
                        _data_offset += blk_info.BlkSize;
                        _size -= blk_info.BlkSize;
                        _offset = 0;
                        _index ++;
                    }

                    object_blk_info.Blks = object_blk_lst;
                    _return.__set_ObjectBlksInfo(object_blk_info);
                    _return.Result = BFSS_RESULT::BFSS_SUCCESS;
                },
                make_document(kvp("OId", oid), kvp("Complete", true)));
            FUNCTION_EXIT_DEBUG_LOG(logger, oid);
        } CATCH_BFSS_EXCEPTION_RESULT(_return);
    }
    BFSS_RESULT::type BFSS_APIDHandler::ResizeObject(const std::string& oid, const int32_t newsize) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, oid, newsize);
        try {
            if (!check_oid(oid) || newsize <= 0 || newsize > object_max_size) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR,"param error!");
            }
            operate_dispersed_cache(
                    [&](object_cache &cache)  {

                        if (cache.Complete) {
                            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_COMPLETED, "The object have been Completed. oid=" + oid);
                        }

                        if (cache.Size < newsize) {
                            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "oldsize < newsize. oid=" + oid);
                        }

                        if (cache.Size == newsize) {
                            return;
                        }

                        auto OldSize = cache.Size;

                        cache.Size = newsize;
                        auto _fbi = build_cache_context(cache);
                        _fbi->Flags = 1; /*force re-do hash calculate.*/

                        if((size_t)std::min(object_head_len, newsize) != cache.head.length()) {
                            cache.head.resize((size_t) std::min(object_head_len, newsize));
                        }
                        execute_mongo_by_object([&](auto collection) {
                            std::string hex;
                            return collection.update_one(
                                    make_document(kvp("OId", oid)),
                                    make_document(kvp("$set",
                                                      make_document(kvp("Size", newsize),
                                                                    kvp("Head", bfss::bin2hex(cache.head, hex))))));
                        });
                        int32_t newPos = data_align(newsize);
                        if (newPos >= OldSize) {
                            return;
                        }

                        BFSS_REGM::ALLOCATE_BLKS_INFO blkInfo;
                        blkInfo.VolumeId = cache.VolumeId;
                        blkInfo.BeginIndex = get_blk_index(cache.BeginIndex, cache.BeginOffset, newPos);
                        blkInfo.BeginOffset = get_blk_end_offset(cache.BeginIndex, cache.BeginOffset, newPos);

                        int32_t oldPos = data_align(OldSize);
                        blkInfo.EndIndex = get_blk_index(cache.BeginIndex, cache.BeginOffset, oldPos);
                        blkInfo.EndOffset = get_blk_end_offset(cache.BeginIndex, cache.BeginOffset, oldPos);

                        add_blk_chip(blkInfo);
                    },
                    oid);
            FUNCTION_EXIT_DEBUG_LOG(logger, oid, newsize);
            return BFSS_RESULT::BFSS_SUCCESS;
        } CATCH_BFSS_EXCEPTION_RETURN();
    }
    BFSS_RESULT::type BFSS_APIDHandler::CreateObjectLink(const std::string& oid, const std::string& hash, const int32_t size, const std::string& head, const int32_t flag, const std::string& tag) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, oid, hash, size, head);
        try {
            if (size <= 0 || size > object_max_size || head.size() > object_head_len ||
                flag & 0xffff0000 || tag.length() > tag_max_len) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR,"param error!");
            }

            mongocxx::options::find_one_and_update find_options{};
            find_options.return_document(mongocxx::options::return_document::k_after);
            find_options.upsert(false);
            std::string hex_head;
            if (auto doc = execute_mongo_by_object_hash([&](auto collection){
                return collection.find_one_and_update(
                    make_document(
                            kvp("Hash", hash),
                            kvp("Size", size),
                            kvp("Head", bin2hex(head,hex_head))),
                    make_document(
                            kvp("$inc", make_document(kvp("RefCount", 1))),
                            kvp("$inc", make_document(kvp("_RefCount", 1))),
                            kvp("$addToSet", make_document(kvp("OIdArray", oid)))),
                    find_options);})) {
                bfss_try([&](){
                    execute_mongo_by_object([&](auto collection){
                        collection.insert_one(make_document(kvp("OId", oid)));
                        return collection.update_one(
                            make_document(kvp("OId", oid)),
                            make_document(
                                    kvp("$set",
                                            make_document(
                                                    kvp("VolumeId", get_mongo_document_value_t(doc,"VolumeId",get_int32())),
                                                    kvp("BeginIndex", get_mongo_document_value_t(doc,"BeginIndex",get_int32())),
                                                    kvp("Offset", get_mongo_document_value_t(doc,"Offset",get_int32())),
                                                    kvp("Size", size),
                                                    kvp("CreateTime", (int64_t)get_time_stamp()),
                                                    kvp("Tag", tag),
                                                    kvp("Flag", flag),
                                                    kvp("Hash", hash),
                                                    kvp("Head", hex_head),
                                                    kvp("Complete", true)))));});
                },[&](BFSS_RESULT::type type, const char *what) {
                    execute_mongo_by_object_hash([&](auto collection){
                        collection.update_one(
                            make_document(kvp("_id",get_mongo_document_value_t(doc,"_id",get_oid()))),
                            make_document(
                                    kvp("$inc", make_document(kvp("RefCount", -1))),
                                    kvp("$pull",make_document(kvp("OIdArray", oid)))
                            ));});
                    THROW_BFSS_EXCEPTION(type,what);
                });
                FUNCTION_EXIT_DEBUG_LOG(logger, oid, hash, size, head);
                return BFSS_RESULT::BFSS_SUCCESS;
            }
            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_NOTFOUND, " find one none failed!");
        } CATCH_BFSS_EXCEPTION_RETURN();
    }
    void BFSS_APIDHandler::GetObjectBlkKey(BLK_KEY_RESULT &_return, const std::string &oid, const int32_t offset) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, oid, offset);
        try {
            if (!check_oid(oid) || offset < 0) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "param error!");
            }
            operate_dispersed_cache(
                [&](object_cache &cache)  {
                    if (offset >= cache.Size) {
                        THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "Read data offset beyond range.");
                    }

                    int blkIndex = get_blk_index(cache.BeginIndex,cache.BeginOffset,pos_align(offset));

                    BFSS_SN::BLKKEY_RESULT _data;
                    try_call_t(get_local_sn_client(cache.VolumeId),
                               &BFSS_SN::BFSS_SNDClient::GetBlkKey,  _data, blkIndex);
                    _return.Result = static_cast<BFSS_RESULT::type>(_data.Result);
                    _return.__set_BlkKey(_data.Data);
                },
                make_document(kvp("OId", oid), kvp("Complete", true)));
            FUNCTION_EXIT_DEBUG_LOG(logger, oid, offset);
        } CATCH_BFSS_EXCEPTION_RESULT(_return);
    }

    void BFSS_APIDHandler::ManageMessage(MESSAGE_RESULT &_return, const int32_t CmdId, const int32_t Param, const std::string &Data) {
        // Your implementation goes here
        FUNCTION_ENTRY_DEBUG_LOG(logger, CmdId, Param, Data);
    }
}


namespace BFSS_APID{

    int StartServer(const libconfig::Config &config) {

        int service_simplethread = 10;
        int service_bind_port = 9092;
        std::string service_bind_host = "any";
        std::string service_bind_addr = "any:9092";
        std::string mongodb_server_uri = "mongodb://localhost";
        std::string regmd_server_uri = "regmd://localhost:9090";
        std::string memcache_server_uri = "memc://localhost";
        if (config.exists("apid.service_simplethread")) {
            config.lookupValue("apid.service_simplethread", service_simplethread);
        }
        LOG4CXX_INFO(logger, "apid.service_simplethread: " << service_simplethread);

        if (config.exists("apid.regmd_server_uri")) {
            config.lookupValue("apid.regmd_server_uri", regmd_server_uri);
        }
        LOG4CXX_INFO(logger, "apid.regmd_server_uri: " << regmd_server_uri);

        if (config.exists("apid.mongodb_server_uri")) {
            config.lookupValue("apid.mongodb_server_uri", mongodb_server_uri);
        }
        LOG4CXX_INFO(logger, "apid.mongodb_server_uri: " << mongodb_server_uri);

        if (config.exists("apid.memcache_server_uri")) {
            config.lookupValue("apid.memcache_server_uri", memcache_server_uri);
        }
        LOG4CXX_INFO(logger, "apid.memcache_server_uri: " << memcache_server_uri);

        if (config.exists("apid.service_bind_addr")) {
            config.lookupValue("apid.service_bind_addr", service_bind_addr);
        }

        LOG4CXX_INFO(logger, "apid.service_bind_addr: " << service_bind_addr);

        std::tie(service_bind_host, service_bind_port) = bfss::parse_address(service_bind_addr);
        // instance mongocxx
        mongocxx::instance instance{};
        shared_ptr<BFSS_API::BFSS_APIDHandler> handler(
                new BFSS_API::BFSS_APIDHandler(mongodb_server_uri, regmd_server_uri, memcache_server_uri));
        shared_ptr<TProcessor> processor(new BFSS_APIDProcessor(handler));
        shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory(_1M_SIZE, 1024, true, true));
        shared_ptr<TNonblockingServerSocket> serverSocket(
                service_bind_host == "any" ?
                new TNonblockingServerSocket(service_bind_port) :
                new TNonblockingServerSocket(service_bind_host, service_bind_port));

        shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(service_simplethread);
        shared_ptr<PosixThreadFactory> threadFactory = std::make_shared<PosixThreadFactory>(new PosixThreadFactory());
        threadManager->threadFactory(threadFactory);
        threadManager->start();
        shared_ptr<TNonblockingServer> server = std::make_shared<TNonblockingServer>(processor, protocolFactory,
                                                                                     serverSocket, threadManager);

        auto _stop_handle = [&server, &threadManager]() -> bool {
            threadManager->stop();
            server->stop();
            return true;
        };

        bfss::bfss_signal _signal;
        _signal.register_handle(SIGABRT, _stop_handle);
        _signal.register_handle(SIGINT, _stop_handle);
        _signal.register_handle(SIGTERM, _stop_handle);
        _signal.register_handle(SIGQUIT, _stop_handle);

        LOG4CXX_INFO(logger, "APID Server is running ...");
        server->serve();
    }
}