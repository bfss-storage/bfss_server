//
// Created by root on 19-2-8.
//

#include "bfss_sn.h"
#include <bfss_utils.h>
#include <functional>
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

using namespace bfss;


namespace  BFSS_SN {

    thread_local regmd_client_ptr _regmd_client(nullptr);


    BFSS_REGM::BFSS_REGMDClient* BFSS_SNDHandler::get_local_regmd_client() {
        if ( !_regmd_client ) {
            auto [rgmd_scheme,_rgmd_host_port] = bfss::parse_uri(_regmd_uri,9090);
            if(rgmd_scheme != "regmd"){
                THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_SCHEME_ERROR, "regmd server scheme error!");
            }
            int _rgmd_port;
            std::string _rgmd_host;
            std::tie(_rgmd_host,_rgmd_port) = _rgmd_host_port;
            shared_ptr<TTransport> socket(new TSocket(_rgmd_host,_rgmd_port));
            shared_ptr<TTransport> transport(new TFramedTransport(socket));
            shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
            _regmd_client = std::make_shared<BFSS_REGM::BFSS_REGMDClient>(protocol);
            transport->open();
        }
        return _regmd_client.get();
    }

    BFSS_SNDHandler::BFSS_SNDHandler(const std::string &mongodb_uri,
                                    const std::string &regmd_uri,
                                    const std::string &volume_name,
                                    int volume_id,
                                    int64_t max_block_size,
                                    int64_t max_cache_size,
                                    const std::string &desc,
                                    const std::string &remote_addr)
            : _regmd_uri(regmd_uri),_key_mgr(mongodb_uri, volume_id) {

        int32_t _blk_count = _cache_mgr.init(volume_name, max_block_size, max_cache_size, _key_mgr);

        BFSS_REGM::REGISTER_VOLUME_INFO volume_info;
        volume_info.VolumeId = volume_id;
        volume_info.Desc = desc;
        volume_info.Uri = remote_addr;
        volume_info.BlkCount = _blk_count;
        GetVersion(volume_info.Version);
        get_local_regmd_client()->RegisterVolume(volume_info);
    }

    BFSS_SNDHandler::~BFSS_SNDHandler() {
        _cache_mgr.flush_and_stop();
    }

    void BFSS_SNDHandler::GetVersion(std::string& _return) {
        /* system.component.major.minor.build.private */
        _return = std::string("bfss.sn.0.0.1.alpha");
    }


    void BFSS_SNDHandler::ManageMessage(MESSAGE_RESULT &_return, const int32_t CmdID, const int32_t Param,
                                        const std::string &Data) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, CmdID, Param);
    }

    void BFSS_SNDHandler::update_hash(const int32_t index, const int32_t offset, const std::string &data,
                                      const bfss::oid_blk_info *fbi){
        FUNCTION_ENTRY_DEBUG_LOG(logger, index, offset);
        if ((!fbi) ||
            (fbi->BeginIndex < 0)  ||
            (fbi->BeginOffset < 0) || (fbi->BeginOffset > blk_size) ||
            (fbi->Size > object_max_size)) {
            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "invalid obi!");
        }

        auto begin_pos = bfss::get_oid_begin_pos(fbi->BeginIndex, fbi->BeginOffset);
        auto write_pos =bfss::get_oid_begin_pos(index, offset);
        if ((index < fbi->BeginIndex) || (begin_pos + fbi->Size < write_pos)) {
            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "invalid index & offset!");
        }

        auto hash_key = bfss::get_blk_hash_node_key(fbi);
        hash_node* hn = get_node__insert(hash_key);

        if ( begin_pos + hn->hashed == write_pos ) {
            SHA256_Update(&hn->sha256, (const void *) data.c_str(), data.length());
            hn->hashed += data.length();
        }
    }

    BFSS_RESULT::type BFSS_SNDHandler::WriteData(const int32_t index, const int32_t offset, const std::string &data,
                                                    const std::string &ctx,
                                                    const int32_t flag) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, index, offset, data.length(), flag);
        if (index < 0 || offset < 0 || offset > blk_size || data.length() < 1) {
            FUNCTION_WARN_LOG(logger, "param error!");
            return BFSS_RESULT::BFSS_PARAM_ERROR;
        }

        try {
            assert(sizeof(bfss::oid_blk_info) == ctx.size());
            auto fbi = (const bfss::oid_blk_info *)ctx.c_str();
            update_hash(index, offset, data, fbi);

            const char *_data = data.c_str();
            auto _length = (int32_t)data.length();
            int32_t _index = index;
            int32_t _offset = offset;

            while (_length > 0) {
                /* 每次只写_index指定的这一个块 */
                int write_size = _cache_mgr.write(_index, _offset, _length, _data);

                _data += write_size;
                _length -= write_size;
                _index ++;
                _offset = 0;
            }

            FUNCTION_EXIT_DEBUG_LOG(logger, index, offset);
            return BFSS_RESULT::BFSS_SUCCESS;
        } CATCH_BFSS_EXCEPTION_RETURN();
    }

    void BFSS_SNDHandler::ReadData(READ_RESULT &_return, const int32_t index, const int32_t offset, const int32_t size,
                                   const int32_t flag) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, index, offset, size, flag);
        if (index < 0 || offset < 0 || offset > blk_size || size < 1 || size > chip_max_size) {
            FUNCTION_WARN_LOG(logger, "param error!");
            _return.Result = BFSS_RESULT::BFSS_PARAM_ERROR;
            return;
        }

        try {
            bool decrypt = (flag & blk_decrypt) == blk_decrypt;

            int32_t _size = size;
            int32_t _index = index;
            int32_t _offset = offset;
            while (_size > 0) {
                /* 每次只读_index指定的这一个块 */
                int read_size = _cache_mgr.read(_index, _offset, _size, _return.Data, decrypt);

                _size -= read_size;
                _index ++;
                _offset = 0;
            }

            _return.__isset.Data = true;
            _return.Result = BFSS_RESULT::BFSS_SUCCESS;
            FUNCTION_EXIT_DEBUG_LOG(logger, index, offset);
        } CATCH_BFSS_EXCEPTION_RESULT(_return);
    }

    static inline int32_t get_index(int32_t BeginIndex, int32_t BeginOffset, int32_t RelativeOffset) {
        /* +1  means the end of one block, treat as the begin of the next block.*/
        return BeginIndex + ((BeginOffset + RelativeOffset + 1) / blk_size);
    }
    static inline int32_t get_offset(int32_t BeginOffset, int32_t RelativeOffset) {
        return (BeginOffset + RelativeOffset) % blk_size;
    }

    hash_node* BFSS_SNDHandler::get_node__insert(int64_t hash_key) {
        boost::unique_lock<boost::shared_mutex> write_lock(_hash_map_mutex);
        hash_node* hn = nullptr;
        auto it = _hash_map.find(hash_key);
        if (it != _hash_map.end()) {
            hn = it->second;
        } else {
            hn = new hash_node();
            _hash_map[hash_key] = hn;
        }
        return hn;
    }
    hash_node* BFSS_SNDHandler::get_node__erase(int64_t hash_key) {
        boost::unique_lock<boost::shared_mutex> write_lock(_hash_map_mutex);
        hash_node* hn = nullptr;
        auto it = _hash_map.find(hash_key);
        if (it != _hash_map.end()) {
            hn = it->second;
            _hash_map.erase(it);
        } else {
            hn = new hash_node();
        }
        return hn;
    }

    void BFSS_SNDHandler::complete_hash(std::string& hash, const bfss::oid_blk_info *obi) {
        if ((!obi) ||
            (obi->BeginIndex < 0)  ||
            (obi->BeginOffset < 0) || (obi->BeginOffset > blk_size) ||
            (obi->Size > object_max_size)) {
            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_PARAM_ERROR, "invalid obi!");
        }
        FUNCTION_ENTRY_DEBUG_LOG(logger, obi->BeginIndex, obi->BeginOffset, obi->Size);

        auto hash_key = bfss::get_blk_hash_node_key(obi);
        hash_node* hn = get_node__erase(hash_key);
        if (obi->Flags == 1) {
            hn->hashed = 0; /*force re-do hash calculate.*/
        }

        int32_t _index  = get_index(obi->BeginIndex, obi->BeginOffset, hn->hashed);
        int32_t _offset = get_offset(obi->BeginOffset, hn->hashed);
        int32_t _size = obi->Size - hn->hashed;
        while (_size > 0) {

            std::string _data;
            int read_size = _cache_mgr.read(_index, _offset, _size, _data, true);

            SHA256_Update(&hn->sha256, (const void *) _data.c_str(), _data.length());
            hn->hashed += read_size;

            _size -= read_size;
            _index ++;
            _offset = 0;
        };

        unsigned char tmp[SHA256_DIGEST_LENGTH];
        SHA256_Final(tmp, &hn->sha256);
        hash.assign((const char *) tmp, SHA256_DIGEST_LENGTH);
        delete hn;
    }
    void BFSS_SNDHandler::CompleteWriteObj(HASH_RESULT &_return, const std::string &ctx) {
        FUNCTION_ENTRY_DEBUG_LOG(logger);
        try {
            assert(sizeof(bfss::oid_blk_info) == ctx.size());
            auto *obi = (const bfss::oid_blk_info *) ctx.c_str();

            complete_hash(_return.hash, obi);

            _return.Result = BFSS_RESULT::BFSS_SUCCESS;
            _return.__set_hash(_return.hash);
            FUNCTION_EXIT_DEBUG_LOG(logger);
        } CATCH_BFSS_EXCEPTION_RESULT(_return);
    }


    void BFSS_SNDHandler::GetBlkKey(BLKKEY_RESULT& _return, const int32_t index) {
        FUNCTION_ENTRY_DEBUG_LOG(logger, index);
		assert(index >= 0);
        try {
            std::string blkKey;
            if (_key_mgr.get_blk_key(index, blkKey)) {
                _return.Result = BFSS_RESULT::BFSS_SUCCESS;
                _return.__set_Data(blkKey);
            } else {
                _return.Result = BFSS_RESULT::BFSS_DATA_UNINITED;
            }
            FUNCTION_EXIT_DEBUG_LOG(logger, index);
        } CATCH_BFSS_EXCEPTION_RESULT(_return);
    }


}

namespace BFSS_SND {
    int StartServer(libconfig::Config &config) {
        int service_simplethread = 10;
        int service_bind_port = 9091;
        int service_volume_id = 0;
        int64_t service_blk_max = -1;
        int64_t max_cache_size = 0;
        std::string service_desc;
        std::string service_bind_host = "any";
        std::string service_bind_addr = "any:9092";
        std::string service_remote_uri = "snd://127.0.0.1:9092";
        std::string mongodb_server_uri = "mongodb://localhost";
        std::string regmd_server_uri = "regmd://localhost:9090";
        std::string blk_dev;

        if(config.exists("snd.service_simplethread")){
            config.lookupValue("snd.service_simplethread",service_simplethread);
        }
        LOG4CXX_INFO(logger, "snd.service_simplethread: " << service_simplethread);

        if(config.exists("snd.service_desc")){
            config.lookupValue("snd.service_desc",service_desc);
        }
        LOG4CXX_INFO(logger, "snd.service_desc: " << service_desc);

        if(config.exists("snd.regmd_server_uri")){
            config.lookupValue("snd.regmd_server_uri",regmd_server_uri);
        }
        LOG4CXX_INFO(logger, "snd.regmd_server_uri: " << regmd_server_uri);

        if(config.exists("snd.mongodb_server_uri")){
            config.lookupValue("snd.mongodb_server_uri",mongodb_server_uri);
        }
        LOG4CXX_INFO(logger, "snd.mongodb_server_uri: " << mongodb_server_uri);

        if(config.exists("snd.service_bind_addr")){
            config.lookupValue("snd.service_bind_addr",service_bind_addr);
        }
        LOG4CXX_INFO(logger, "snd.service_bind_addr: " << service_bind_addr);

        config.lookupValue("snd.service_blk_dev", blk_dev);
        LOG4CXX_INFO(logger, "snd.service_blk_dev: " << blk_dev);

        config.lookupValue("snd.service_volume_id", service_volume_id);
        LOG4CXX_INFO(logger, "snd.service_volume_id: " << service_volume_id);

        if(config.exists("snd.service_blk_max")){
            std::string _max;
            config.lookupValue("snd.service_blk_max",_max);
            service_blk_max = bfss::str2size(_max);
        }

        if(config.exists("snd.max_cache_size")){
            std::string _max;
            config.lookupValue("snd.max_cache_size",_max);
            max_cache_size = bfss::str2size(_max);
        }

        LOG4CXX_INFO(logger, "snd.service_blk_max: " << service_blk_max);

        if(config.exists("snd.max_cache_size")){
            std::string _max;
            config.lookupValue("snd.max_cache_size",_max);
            max_cache_size = bfss::str2size(_max);
        }
        LOG4CXX_INFO(logger, "snd.max_cache_size: " << max_cache_size);

        if(config.exists("snd.service_remote_uri")){
            config.lookupValue("snd.service_remote_uri",service_remote_uri);
        }
        auto [_snd_scheme,_snd_host_port] = bfss::parse_uri(service_remote_uri,-1);
        if(_snd_scheme != "snd"){
            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_SCHEME_ERROR, "snd server scheme error!");
        }

        std::tie(service_bind_host,service_bind_port) = bfss::parse_address(service_bind_addr);

        int _remote_port;
        std::string _remote_host;
        std::tie(_remote_host,_remote_port) = _snd_host_port;

        LOG4CXX_INFO(logger, "snd.service_remote_addr: " << service_remote_uri);

        if(_remote_port == -1){
            _remote_port = service_bind_port;
            service_remote_uri = "snd:://" + _remote_host + ":" + std::to_string(_remote_port);
        }
        if(service_desc.empty()){
            std::stringstream _s;
            char _host[128];
            gethostbyname(_host);
            _s << "snd:{hostname:" << _host <<
                  ",remote_addr:" << _remote_host <<
                  "};";
            service_desc = _s.str();
        }


        mongocxx::instance instance{};
        shared_ptr <BFSS_SN::BFSS_SNDHandler> handler(new BFSS_SN::BFSS_SNDHandler(
                mongodb_server_uri, regmd_server_uri,
                blk_dev, service_volume_id,
                service_blk_max, max_cache_size, service_desc, service_remote_uri));

        shared_ptr <TProcessor> processor(new BFSS_SN::BFSS_SNDProcessor(handler));
        //server
        shared_ptr <TProtocolFactory> protocolFactory(new TBinaryProtocolFactory(_1M_SIZE, 1024, true, true));
        shared_ptr <TNonblockingServerSocket> serverSocket(
                service_bind_host == "any" ?
                new TNonblockingServerSocket(service_bind_port) :
                new TNonblockingServerSocket(service_bind_host, service_bind_port)
                );
        shared_ptr <ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(service_simplethread);
        shared_ptr <PosixThreadFactory> threadFactory = std::make_shared<PosixThreadFactory>(new PosixThreadFactory());
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

        LOG4CXX_INFO(logger, "SND Server is running ...");
        server->serve();
    }
}


