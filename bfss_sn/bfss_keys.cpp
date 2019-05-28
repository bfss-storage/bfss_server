//
// Created by root on 19-2-8.
//

#include "bfss_keys.h"
#include "bfss_utils.h"
#include "bfss_mongo_utils.h"

using namespace bfss;



namespace BFSS_SN {

    keys_manager::keys_manager(const std::string &mongodb_uri, int32_t volume_id)
            :_volume_id(volume_id), _mongo_client_pool(mongocxx::uri(mongodb_uri)) {
        execute_mongo_by_oblect([](auto collection) {
            mongocxx::options::index index_options{};
            index_options.unique(true);
            collection.create_index(make_document(kvp("VolumeId", 1), kvp("BlkIndex", 1)), index_options);
        });
    }

    keys_manager::~keys_manager(){
        boost::unique_lock<boost::shared_mutex> write_lock(_blk_key_map_mutex);
        for (auto it : _blk_key_map) {
            key_context& ctx = it.second;
            EVP_CIPHER_CTX_free(ctx.cipher_ctx);
            ctx.cipher_ctx = nullptr;
        }
        _blk_key_map.clear();
    }

    bool keys_manager::get_blk_key(int index, std::string& bkey) {
        key_context ctx;
        if (get_blk_key(index, ctx)) {
            bkey.assign((char *)ctx.key, BFSS_MIX_KEY_LEN);
            return true;
        }
        return false;
    }


    bool keys_manager::get_blk_key(int index, key_context& ctx) {
        boost::upgrade_lock<boost::shared_mutex> read_lock(_blk_key_map_mutex);

        std::map<int, key_context>::iterator it;
        if (bfss::has_lockup(_blk_key_map, index, it)) {
            ctx = it->second;
            return true;
        }

        unsigned char salt[32] = {0};
        auto _time_stamp = bfss::get_time_stamp();
        ((uint64_t *) salt)[0] = (uint64_t) _time_stamp & (uint64_t&) ctx.iv;
        ((uint64_t *) salt)[1] = (uint64_t) _time_stamp | (uint64_t&) ctx.key;
        ((uint64_t *) salt)[2] = (uint64_t) _time_stamp ^ (uint64_t&) ctx.iv;
        ((uint64_t *) salt)[3] = (uint64_t) _time_stamp & (uint64_t&) ctx.key;
        int salt_result = RAND_bytes(salt, 32);
        if (salt_result != 1) {
            FUNCTION_WARN_LOG(logger, "RAND_bytes failed, error=" << ERR_get_error());
            return false;
        }

        unsigned char data[8] = {0};
        ((int32_t *) data)[0] = _volume_id;
        ((int32_t *) data)[1] = index;
        int i = EVP_BytesToKey(EVP_aes_256_ecb(), EVP_sha1(), salt, data, sizeof(data), 16, ctx.key, ctx.iv);
        if (i != 32) {
            FUNCTION_WARN_LOG(logger, "key size should be 256 bits");
            return false;
        }

        unsigned char bkey[BFSS_MIX_KEY_LEN] = {0};
        memcpy(bkey, ctx.key, BFSS_KEY_LEN);
        memcpy((bkey + BFSS_KEY_LEN), ctx.iv, BFSS_IV_LEN);

        bsoncxx::types::b_binary _key;
        _key.bytes = bkey;
        _key.size = sizeof(bkey);

        int32_t flag = 0xffff0000;
        mongocxx::options::find_one_and_update find_options{};
        find_options.return_document(mongocxx::options::return_document::k_after);
        find_options.upsert(true);

        execute_mongo_by_oblect([&](auto collection) {
            auto doc = collection.find_one_and_update(
                    make_document(kvp("VolumeId", _volume_id), kvp("BlkIndex", index)),
                    make_document(kvp("$setOnInsert",
                                      make_document(kvp("Flag", flag),
                                                    kvp("BKey", _key)))), find_options);

            auto k = doc.value().view()["BKey"].get_binary();

            memcpy(ctx.key, k.bytes, BFSS_KEY_LEN);
            memcpy(ctx.iv, (k.bytes + BFSS_KEY_LEN), BFSS_IV_LEN);
            ctx.cipher_ctx = EVP_CIPHER_CTX_new();
            boost::upgrade_to_unique_lock<boost::shared_mutex> write_lock(read_lock);
            _blk_key_map[index] = ctx;
        });
        return true;
    }


    int keys_manager::encrypt(int32_t index, const unsigned char *data, int data_len,
                                 unsigned char *out, int &out_len) {
        key_context ctx;
        if (get_blk_key(index, ctx)) {
            EVP_EncryptInit_ex(ctx.cipher_ctx, EVP_aes_256_ecb(), nullptr, ctx.key, ctx.iv);
            return EVP_EncryptUpdate(ctx.cipher_ctx, out, &out_len, data, data_len);
        }
        return 0;   /*1 success;*/
    }
    int keys_manager::decrypt(int32_t index, const unsigned char *data, int data_len,
                                 unsigned char *out, int &out_len) {
        key_context ctx;
        if (get_blk_key(index, ctx)) {
            EVP_DecryptInit_ex(ctx.cipher_ctx, EVP_aes_256_ecb(), nullptr, ctx.key, ctx.iv);
            return EVP_DecryptUpdate(ctx.cipher_ctx, out, &out_len, data, data_len);
        }
        return 0;   /*1 success;*/
    }
}


