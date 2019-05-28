//
// Created by root on 19-2-8.
//

#include "bfss_regm.h"
#include <bfss_signal.h>

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

namespace BFSS_REGM {
    const int16_t  CMD_SN_ENABLE = 1;
    const int16_t  CMD_SN_DISABLE = 2;

    BFSS_REGMDHandler::BFSS_REGMDHandler(const std::string &volume_mongo_uri)
            : _mongo_client_pool(mongocxx::uri(volume_mongo_uri)) {
        _init_db_collection();

        _sn_find_thread = std::make_shared<std::thread>(std::bind(&BFSS_REGMDHandler::_sn_find, this));
    }

    BFSS_REGMDHandler::~BFSS_REGMDHandler() {
        _condition.notify_all();
        _sn_find_thread->join();
    }

    void BFSS_REGMDHandler::GetVersion(std::string &_return) {
        /* system.component.major.minor.build.private */
        _return = std::string("bfss.regm.0.0.1.alpha");
    }

    void BFSS_REGMDHandler::ManageMessage(MESSAGE_REGM_RESULT &_return, const int32_t CmdId, const int32_t Param, const std::string &Data) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, CmdId);
        try {
            switch (CmdId) {
                case CMD_SN_ENABLE: {/* SN节点使能 */
                    auto doc = execute_mongo_by_volumecol([&](auto collection) {
                        return collection.find_one_and_update(
                                make_document(kvp("VolumeId", Param)),
                                make_document(kvp("$set", make_document(kvp("isOnline", true)))));
                    });
                    if (doc) {
                        _rebuild_sn_queue();
                        _return.Result = BFSS_RESULT::BFSS_SUCCESS;
                    } else {
                        _return.Result = BFSS_RESULT::BFSS_NOTFOUND;
                    }
                    FUNCTION_WARN_LOG(logger, "CMD_SN_ENABLE: " << Param << " - " << Data);
                    return;
                }
                case CMD_SN_DISABLE: {/* SN节点禁用 */
                    auto doc = execute_mongo_by_volumecol([&](auto collection) {
                        return collection.find_one_and_update(
                                make_document(kvp("VolumeId", Param)),
                                make_document(kvp("$set", make_document(kvp("isOnline", false)))));
                    });
                    if (doc) {
                        _rebuild_sn_queue();
                        _return.Result = BFSS_RESULT::BFSS_SUCCESS;
                    } else {
                        _return.Result = BFSS_RESULT::BFSS_NOTFOUND;
                    }
                    FUNCTION_WARN_LOG(logger, "CMD_SN_DISABLE: " << Param << " - " << Data);
                    return;
                }
                default:
                    break;
            }
        } CATCH_BFSS_EXCEPTION_RESULT(_return);
    }

    void BFSS_REGMDHandler::_init_db_collection() {
        mongocxx::options::index index_options{};
        index_options.unique(true);
        execute_mongo_by_volumecol([&index_options](auto collection) {
            collection.create_index(make_document(kvp("VolumeId", 1)), index_options);
            collection.create_index(make_document(kvp("VolumeId", 1), kvp("Reserved", 1)));
        });
        execute_mongo_by_volumechipcol([&index_options](auto collection) {
            collection.create_index(make_document(kvp("VolumeId", 1),
                                                  kvp("BeginOffset", 1),
                                                  kvp("BeginIndex", 1),
                                                  kvp("Lock", 1)), index_options);
        });
    }

    BFSS_RESULT::type BFSS_REGMDHandler::RegisterVolume(const REGISTER_VOLUME_INFO &VolumeInfo) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, VolumeInfo);
        try {
            if (VolumeInfo.BlkCount < 0 || VolumeInfo.VolumeId <= 0) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "param error!");
            }
            mongocxx::options::find_one_and_update find_options{};
            find_options.return_document(mongocxx::options::return_document::k_before);
            find_options.upsert(true);

            auto before_doc = execute_mongo_by_volumecol([&](auto collection) {
                return collection.find_one_and_update(
                        make_document(kvp("VolumeId", VolumeInfo.VolumeId)),
                        make_document(kvp("$setOnInsert",
                                          make_document(kvp("ServerUri", VolumeInfo.Uri),
                                                        kvp("BlkCount", VolumeInfo.BlkCount),
                                                        kvp("Desc", VolumeInfo.Desc),
                                                        kvp("CurPosition", (int64_t) 0),
                                                        kvp("Reserved", (int64_t) VolumeInfo.BlkCount * blk_size)))),
                        find_options);
            });

            if (before_doc && get_mongo_document_value_t(before_doc, "CurPosition", get_int64())
                              > ((int64_t) VolumeInfo.BlkCount * blk_size)) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_SCHEME_ERROR, "CUT-OFF volume is unsupported !");
            }

            execute_mongo_by_volumecol([&](auto collection) {
                return collection.find_one_and_update(
                        make_document(kvp("VolumeId", VolumeInfo.VolumeId)),
                        make_document(kvp("$set", make_document(kvp("isOnline", true)))));
            });

            FUNCTION_EXIT_DEBUG_LOG(logger);
            return BFSS_RESULT::BFSS_SUCCESS;
        } CATCH_BFSS_EXCEPTION_RETURN();
    }

    void BFSS_REGMDHandler::GetVolumeInfo(VOLUME_RESULT &_return, const int32_t VolumeId) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, VolumeId);
        try {
            if (VolumeId < 1) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "VolumeId < 1!");
            }
            if (auto doc = execute_mongo_by_volumecol([&VolumeId](auto collection) {
                return collection.find_one(make_document(kvp("VolumeId", VolumeId)));
            })) {
                int64_t cpos;
                VOLUME_INFO info;
                auto validate =
                        get_mongo_document_value(doc, "ServerUri", info.Uri) &&
                        get_mongo_document_value(doc, "VolumeId", info.VolumeId) &&
                        get_mongo_document_value(doc, "BlkCount", info.BlkCount) &&
                        get_mongo_document_value(doc, "CurPosition", cpos);
                assert(validate);

                get_mongo_document_value(doc, "Desc", info.Desc);
                info.UsedCount = int(cpos / blk_size);
                _return.__set_Info(info);
                _return.Result = BFSS_RESULT::BFSS_SUCCESS;
            } else {
                FUNCTION_WARN_LOG(logger, "find one failed!");
                _return.Result = BFSS_RESULT::BFSS_NOTFOUND;
                return;
            }
            FUNCTION_EXIT_DEBUG_LOG(logger);
        } CATCH_BFSS_EXCEPTION_RESULT(_return);
    }

    void BFSS_REGMDHandler::GetAllVolumeInfo(std::vector<VOLUME_INFO> &_return) {
        FUNCTION_ENTRY_DEBUG_LOG(logger);
        try {
            VOLUME_INFO info;
            auto all_volumes = execute_mongo_by_volumecol([](auto collection) {
                return collection.find(make_document(kvp("VolumeId", make_document(kvp("$exists", true)))));
            });
            for (auto volume : all_volumes) {
                int64_t cpos = 0;
                auto validate =
                        get_mongo_document_value(volume, "ServerUri", info.Uri) &&
                        get_mongo_document_value(volume, "VolumeId", info.VolumeId) &&
                        get_mongo_document_value(volume, "BlkCount", info.BlkCount) &&
                        get_mongo_document_value(volume, "CurPosition", cpos);
                assert(validate);
                get_mongo_document_value(volume, "Desc", info.Desc);
                info.UsedCount = int(cpos / blk_size);
                _return.push_back(info);
            }
            FUNCTION_EXIT_DEBUG_LOG(logger);
        } catch (std::exception &e) {
            EXCEPTION_ERROR_LOG(logger, e);
        }
    }

    BFSS_RESULT::type BFSS_REGMDHandler::AddBlkChip(const ALLOCATE_BLKS_INFO &blksInfo) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, blksInfo);
        try {
            if ((blksInfo.BeginIndex < 0) ||
                (blksInfo.EndIndex < 0) ||
                ((blksInfo.EndIndex - blksInfo.BeginIndex) < 0) ||
                (blksInfo.EndOffset < 0) ||
                (blksInfo.BeginOffset > blk_size) ||
                (blksInfo.EndOffset > blk_size) ||
                (data_align(blksInfo.BeginOffset) != blksInfo.BeginOffset ) ||
                (data_align(blksInfo.EndOffset) != blksInfo.EndOffset) ) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "bfss_param_error!");
            }

            int64_t Size = calc_object_size(blksInfo);

            auto BeginIndex = blksInfo.BeginIndex;
            auto EndIndex = blksInfo.EndIndex;

            auto BeginOffset = blksInfo.BeginOffset;
            auto EndOffset = blksInfo.EndOffset;

            /*
             * 向后合并碎片
             */
            while (auto doc = execute_mongo_by_volumechipcol([&](auto collection) {
                return collection.find_one_and_delete(make_document(
                        kvp("VolumeId", blksInfo.VolumeId),
                        kvp("BeginOffset", EndOffset),
                        kvp("BeginIndex", EndIndex),
                        kvp("Lock", false)));
            })) {
                Size += get_mongo_document_value_t(doc, "Size", get_int64());
                EndOffset = get_blk_end_offset(blksInfo.BeginIndex, blksInfo.BeginOffset, Size);
                EndIndex = get_blk_index(blksInfo.BeginIndex, blksInfo.BeginOffset, Size);
            }
            /*
             * 向前合并碎片
             */
            while (auto doc = execute_mongo_by_volumechipcol([&](auto collection) {
                return collection.find_one_and_delete(make_document(
                        kvp("VolumeId", blksInfo.VolumeId),
                        kvp("EndOffset", BeginOffset),
                        kvp("EndIndex", BeginIndex),
                        kvp("Lock", false)));
            })) {
                Size += get_mongo_document_value_t(doc, "Size", get_int64());
                BeginIndex = get_mongo_document_value_t(doc, "BeginIndex", get_int32());
                BeginOffset = get_mongo_document_value_t(doc, "BeginOffset", get_int32());
            }
            /*
             * 插入合并后的数据
             */
            execute_mongo_by_volumechipcol([&](auto collection) {
                return collection.insert_one(make_document(
                        kvp("BeginOffset", BeginOffset),
                        kvp("BeginIndex", BeginIndex),
                        kvp("EndOffset", EndOffset),
                        kvp("EndIndex", EndIndex),
                        kvp("Lock", false),
                        kvp("VolumeId", blksInfo.VolumeId),
                        kvp("Size", Size)));
            });

            FUNCTION_EXIT_DEBUG_LOG(logger, blksInfo.VolumeId, BeginIndex, BeginOffset, EndIndex, EndOffset);
            return BFSS_RESULT::BFSS_SUCCESS;
        } CATCH_BFSS_EXCEPTION_RETURN();
    }

    void BFSS_REGMDHandler::AllocateBlks(ALLOCATE_BLKS_RESULT &_return, const int32_t size) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, size);
        try {
            if (size <= 0 || size > object_max_size) {
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "bfss_param_error!");
            }
            int32_t alloc_size = data_align(size);
            mongocxx::options::find_one_and_update find_options{};
            find_options.return_document(mongocxx::options::return_document::k_before);
            if (auto doc = execute_mongo_by_volumechipcol([&](auto collection) {
                return collection.find_one_and_update(
                        make_document(kvp("Size", make_document(kvp("$gte", (int64_t) alloc_size))),
                                      kvp("Lock", false)),
                        make_document(kvp("$inc", make_document(kvp("Size", (int64_t) -alloc_size))),
                                      kvp("$set", make_document(kvp("Lock", true)))), find_options);
            })) {
                auto VolumeId = get_mongo_document_value_t(doc, "VolumeId", get_int32());
                auto BeginIndex = get_mongo_document_value_t(doc, "BeginIndex", get_int32());
                auto BeginOffset = get_mongo_document_value_t(doc, "BeginOffset", get_int32());
                auto EndIndex = get_mongo_document_value_t(doc, "EndIndex", get_int32());
                auto EndOffset = get_mongo_document_value_t(doc, "EndOffset", get_int32());
                /*
                 * 断言检测数据
                 */
                assert(get_oid_size(BeginIndex, BeginOffset, EndIndex, EndOffset) >= alloc_size);

                ALLOCATE_BLKS_INFO BlksInfo;
                BlksInfo.BeginIndex = get_blk_index(EndIndex, EndOffset, -alloc_size);
                BlksInfo.BeginOffset = get_blk_end_offset(get_oid_end_pos(EndIndex, EndOffset, -alloc_size));
                BlksInfo.EndIndex = EndIndex;
                BlksInfo.EndOffset = EndOffset;
                BlksInfo.VolumeId = VolumeId;
                _return.__set_BlksInfo(BlksInfo);
                _return.Result = BFSS_RESULT::BFSS_SUCCESS;

                int64_t Before_Size = get_mongo_document_value_t(doc, "Size", get_int64());
                int64_t New_Size = Before_Size - (int64_t) alloc_size;
                if (New_Size > 0) {
                    /*
                     * 更新数据库
                     */
                    execute_mongo_by_volumechipcol([&](auto collection) {
                        collection.update_one(
                                make_document(kvp("_id", get_mongo_document_value_t(doc, "_id", get_oid())),
                                              kvp("Lock", true)),
                                make_document(kvp("$set",
                                                  make_document(
                                                          kvp("EndIndex", BlksInfo.BeginIndex),
                                                          kvp("EndOffset", BlksInfo.BeginOffset),
                                                          kvp("Lock", false)))));
                    });
                    FUNCTION_DEBUG_LOG(logger, "Chip realloc orig size=" << Before_Size << ", new size=" << New_Size);
                } else {
                    /*
                     * 这个节点已被分配空了，就删除掉
                     */
                    execute_mongo_by_volumechipcol([&](auto collection) {
                        collection.delete_one(
                                make_document(kvp("_id", get_mongo_document_value_t(doc, "_id", get_oid())),
                                              kvp("Lock", true)));
                        FUNCTION_DEBUG_LOG(logger,
                                           "Chip delete.  orig size=" << Before_Size << ", new size=" << New_Size);
                    });
                }

                FUNCTION_EXIT_DEBUG_LOG(logger, size, BlksInfo.VolumeId, BlksInfo.BeginIndex, BlksInfo.BeginOffset,
                                        BlksInfo.EndIndex, BlksInfo.EndOffset);
            } else {
                size_t loopCount = _get_volume_count();
                while (loopCount > 0) {
                    int volumeId = _get_volume_id();
                    find_options.return_document(mongocxx::options::return_document::k_before);
                    auto _doc = execute_mongo_by_volumecol([&](auto collection) {
                        return collection.find_one_and_update(
                                make_document(kvp("VolumeId", volumeId),
                                              kvp("Reserved", make_document(kvp("$gte", (int64_t) (alloc_size))))),
                                make_document(kvp("$inc", make_document(
                                        kvp("CurPosition", (int64_t) (alloc_size)),
                                        kvp("Reserved", (int64_t) (-alloc_size))))),
                                find_options);
                    });
                    if (_doc) {
                        int64_t curp = get_mongo_document_value_t(_doc, "CurPosition", get_int64());
                        assert(curp == data_align(curp));

                        ALLOCATE_BLKS_INFO blksinfo;
                        blksinfo.VolumeId = volumeId;
                        blksinfo.BeginOffset = get_blk_begin_offset(curp);
                        blksinfo.BeginIndex = get_blk_index(curp);
                        blksinfo.EndOffset = get_blk_end_offset(curp + alloc_size);
                        blksinfo.EndIndex = get_blk_index(curp + alloc_size);

                        _return.__set_BlksInfo(blksinfo);

                        _return.Result = BFSS_RESULT::BFSS_SUCCESS;
                        FUNCTION_EXIT_DEBUG_LOG(logger, size, blksinfo.VolumeId, blksinfo.BeginIndex,
                                                blksinfo.BeginOffset, blksinfo.EndIndex, blksinfo.EndOffset);
                        return;
                    }

                    loopCount--;
                }
                if (loopCount == 0) {
                    THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_NO_SPACE, "no space  in all volumes.");
                }
            }
        } CATCH_BFSS_EXCEPTION_RESULT(_return);
    }

    void BFSS_REGMDHandler::_rebuild_sn_queue() {
        try {
            std::lock_guard<std::mutex> lock(_sn_client_rwlock);

            auto _master_docs = execute_mongo_by_volumecol([](auto collection) {
                return collection.find(make_document(kvp("isOnline", true)));
            });

            _sn_client_queue = std::queue<int32_t>();
            for (auto &doc : _master_docs) {
                int32_t volume_id;
                get_mongo_document_value(doc, "VolumeId", volume_id);
                _sn_client_queue.push(volume_id);
            }
        } catch (std::exception &e) {
            LOG4CXX_ERROR(logger, "_rebuild_sn_queue catch exception:" << e.what());
        };
    }

    void BFSS_REGMDHandler::_sn_find() {
        while (1) {
            _rebuild_sn_queue();

            std::unique_lock lock(_condition_mutex);
            if (std::cv_status::no_timeout == _condition.wait_for(lock, std::chrono::milliseconds(30 * 1000))) {
                break;
            }
        }
    }

    size_t BFSS_REGMDHandler::_get_volume_count() {
        std::lock_guard<std::mutex> lock(_sn_client_rwlock);
        return _sn_client_queue.size();
    }

    int32_t BFSS_REGMDHandler::_get_volume_id() {
        std::lock_guard<std::mutex> lock(_sn_client_rwlock);
        int volumeID = _sn_client_queue.front();
        _sn_client_queue.pop();
        _sn_client_queue.push(volumeID);
        assert(volumeID > 0);
        return volumeID;
    }

}

namespace BFSS_REGMD {
    int StartServer(const libconfig::Config &config) {

        int service_simplethread = 10;
        int service_bind_port = 9090;
        std::string service_bind_host = "any";
        std::string service_bind_addr = "any:9092";
        std::string mongodb_server_uri = "mongodb://localhost";

        if(config.exists("regmd.service_simplethread")){
            config.lookupValue("apid.service_simplethread",service_simplethread);
        }

        LOG4CXX_INFO(logger, "regmd.service_simplethread: " << service_simplethread);

        if(config.exists("regmd.mongodb_server_uri")){
            config.lookupValue("regmd.mongodb_server_uri",mongodb_server_uri);
        }

        LOG4CXX_INFO(logger, "regmd.mongodb_server_uri: " << mongodb_server_uri);

        if(config.exists("regmd.service_bind_addr")){
            config.lookupValue("regmd.service_bind_addr",service_bind_addr);
        }
        LOG4CXX_INFO(logger, "regmd.service_bind_addr: " << service_bind_addr);

        std::tie(service_bind_host,service_bind_port) = bfss::parse_address(service_bind_addr);

        mongocxx::instance instance{};

        shared_ptr<BFSS_REGM::BFSS_REGMDHandler> handler(new BFSS_REGM::BFSS_REGMDHandler(mongodb_server_uri));
        shared_ptr<TProcessor> processor(new BFSS_REGM::BFSS_REGMDProcessor(handler));
        //server
        shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory(_1M_SIZE, 1024, true, true));
        shared_ptr<TNonblockingServerSocket> serverSocket(service_bind_host == "any" ?
                new TNonblockingServerSocket(service_bind_port) :
                new TNonblockingServerSocket(service_bind_host, service_bind_port));

        shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(service_simplethread);
        shared_ptr<PosixThreadFactory> threadFactory = std::make_shared<PosixThreadFactory>(new PosixThreadFactory());
        threadManager->threadFactory(threadFactory);
        threadManager->start();

        shared_ptr<TNonblockingServer> server = std::make_shared<TNonblockingServer>(processor, protocolFactory, serverSocket, threadManager);
        auto _stop_handle = [&server,&threadManager]()->bool {
            threadManager->stop();
            server->stop();
            return true;
        };

        bfss::bfss_signal _signal;
        _signal.register_handle(SIGABRT,_stop_handle);
        _signal.register_handle(SIGINT,_stop_handle);
        _signal.register_handle(SIGTERM,_stop_handle);
        _signal.register_handle(SIGQUIT,_stop_handle);

        LOG4CXX_INFO(logger, "REGD Server is running ...");
        server->serve();
        return 0;
    }
}