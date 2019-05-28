//
// Created by root on 19-2-8.
//

#ifndef BFSS__SN__KEYS__H
#define BFSS__SN__KEYS__H


#include <bsoncxx/json.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/cursor.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/pool.hpp>

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


#include <boost/thread/thread.hpp>
#include <boost/serialization/singleton.hpp>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <bfss_base.h>
#include <bfss_exception.h>
#include "blkdev.h"


namespace  BFSS_SN {

    class keys_manager {
#define BFSS_KEY_LEN        32
#define BFSS_IV_LEN         EVP_MAX_IV_LENGTH   /*16*/
#define BFSS_MIX_KEY_LEN    (BFSS_KEY_LEN + BFSS_IV_LEN)
        struct key_context{
            unsigned char    key[BFSS_KEY_LEN];
            unsigned char    iv[BFSS_IV_LEN];
            EVP_CIPHER_CTX * cipher_ctx;
            key_context() {
                memset(key, 0, BFSS_KEY_LEN);
                memset(key, 0, BFSS_IV_LEN);
                cipher_ctx = nullptr;
            }
        };
    private:
        template<typename _execute_fun_t>
        auto execute_mongo_by_oblect(_execute_fun_t _fun) {
            auto _entry = _mongo_client_pool.acquire();
            return _fun((*_entry).database("BKeyDb").collection("BKeyCol"));
        }

    public:
        keys_manager(const std::string &mongodb_uri, int32_t volume_id);
        ~keys_manager();

        bool get_blk_key(int index, std::string& bkey);

        int encrypt(int32_t index, const unsigned char *data, int data_len,
                                         unsigned char *out,  int &out_len);
        int decrypt(int32_t index, const unsigned char *data, int data_len,
                                         unsigned char *out,  int &out_len);
    private:
        bool get_blk_key(int index, key_context& ctx);

        mongocxx::pool            _mongo_client_pool;

        std::map<int, key_context> _blk_key_map;
        boost::shared_mutex       _blk_key_map_mutex;

        int32_t                   _volume_id;
    };

}


#endif //BFSS__SN__KEYS__H
