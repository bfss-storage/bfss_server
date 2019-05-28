//
// Created by root on 19-2-8.
//

#include "bfss_node.h"
using namespace bfss;



namespace BFSS_SN {

    int32_t cache_node::write(const int32_t index, const int32_t offset, const int32_t size, const char *data) {
        assert(encrypted != nullptr);
        assert(blk_index == index);
        assert(status != NS_UNINITED);

        int32_t _real_size = size;
        if ((size + offset) > blk_size) {
            _real_size = blk_size - offset;
        }

        boost::shared_lock<boost::shared_mutex> read_lock(blk_mutex);   /*cache要能支持并发写*/
        char *_cache_pos = blk_data + offset;
        memcpy(_cache_pos, data, (size_t) _real_size);
        status = NS_UPDATED;
        mtime = time(nullptr);
        return _real_size;
    }

    int32_t cache_node::read(const int32_t index, const int32_t offset, const int32_t size, std::string &data, bool decrypt) {
        assert(decrypt == (encrypted != nullptr));
        assert(blk_index == index);
        assert(status != NS_UNINITED);

        int32_t _real_size = size;
        if ((size + offset) > blk_size) {
            _real_size = blk_size - offset;
        }

        boost::shared_lock<boost::shared_mutex> read_lock(blk_mutex);
        char *_cache_pos = blk_data + offset;
        data.append(_cache_pos, (size_t) _real_size);
        return _real_size;
    }

    bool cache_node::blk2cache() {
        boost::unique_lock<boost::shared_mutex> write_lock(blk_mutex);
        assert(encrypted == nullptr);
        assert(status == NS_UNINITED);

        bool ret = _s_blk_device.readblk(blk_index, blk_data);
        if (ret) {
            FUNCTION_DEBUG_LOG(logger, "blk2cache(" << blk_index << ")  " << to_string());
        } else {
            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_DATA_READ_FAILED, to_string() + " _blk_device.readblk() failed.");
        }
        status = NS_INITED;
        return ret;
    }

    bool cache_node::cache2blk() {
        boost::shared_lock<boost::shared_mutex> read_lock(blk_mutex);
        assert(encrypted == nullptr);
        assert(status == NS_UPDATED);

        bool ret = _s_blk_device.writeblk(blk_index, blk_data);
        if (ret) {
            FUNCTION_DEBUG_LOG(logger, "cache2blk(" << blk_index << ")  " << to_string());
        } else {
            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_DATA_WRITE_FAILED, to_string() + " _blk_device.writeblk() failed.");
        }
        status = NS_FLUSHED;
        return ret;
    }

    bool cache_node::encrypt() {
        assert(encrypted != nullptr);
        assert(encrypted != this);
        assert(encrypted->blk_index == blk_index);

        boost::shared_lock<boost::shared_mutex> read_lock(blk_mutex);
        boost::unique_lock<boost::shared_mutex> write_lock(encrypted->blk_mutex);

        int out_len = 0;
        int ret = key_mgr.encrypt(blk_index, (unsigned char *) blk_data, blk_size,
                                  (unsigned char *) encrypted->blk_data, out_len);
        if (ret != 1) {
            FUNCTION_WARN_LOG(logger, "encrypt for " << to_string() << " failed with " << ret);
            return false;
        }
        status = NS_FLUSHED;

        encrypted->status = NS_UPDATED;
        if (mtime != INT64_MAX) {
            mtime = INT64_MAX;
            encrypted->mtime = time(nullptr);
        }
        FUNCTION_DEBUG_LOG(logger, "encrypt from " << to_string() << " to " << encrypted->to_string());
        return (out_len == blk_size);
    }

    bool cache_node::decrypt() {
        assert(encrypted != nullptr);
        assert(encrypted != this);
        assert(encrypted->blk_index == blk_index);

        boost::shared_lock<boost::shared_mutex> read_lock(encrypted->blk_mutex);
        boost::unique_lock<boost::shared_mutex> write_lock(blk_mutex);

        int out_len = 0;
        int ret = key_mgr.decrypt(blk_index, (unsigned char *) encrypted->blk_data, blk_size,
                                  (unsigned char *) blk_data, out_len);
        if (ret != 1) {
            FUNCTION_WARN_LOG(logger, "decrypt for " << encrypted->to_string() << " failed with " << ret);
            return false;
        }
        encrypted->status = NS_FLUSHED;
        // assert(out_len == blk_size); not equal always...

        status = NS_INITED;
        FUNCTION_DEBUG_LOG(logger, "decrypt from " << encrypted->to_string() << " to " << to_string());
        return true;
    }

    void cache_node::reset_index(int32_t index, cache_node *encrypted_node) {
        boost::unique_lock<boost::shared_mutex> write_lock(blk_mutex);
        assert(status != NS_UPDATED);
        std::string old = to_string();
        blk_index = index;
        status = NS_UNINITED; // reset status for new block.
        encrypted = encrypted_node;
        FUNCTION_DEBUG_LOG(logger, "reset_index " << old << " to " << to_string());
    }


/////////////////////////////////////////////////////////////////////////////////////////////////////

    void lru_indexable_list::init(int32_t _blk_count, int64_t max_cache_size, keys_manager &_key_mgr) {
        _cache_index = new lru_index[_blk_count];

        if (max_cache_size < cache_size) {
            max_cache_size = cache_size;
        }
        _cache_ptr = bfss::alloc_cache(cache_size, max_cache_size);
        if (!_cache_ptr) {
            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_NO_MEMORY, " alloc_cache failed!");
        }

        auto _cache_count = _cache_ptr->get_count<sizeof(cache_node)>();
        for (auto i = 0; i < _cache_count; i++) {
            new(_cache_ptr->get_blk<cache_node>(i)) cache_node(_key_mgr, i);
        }

        boost::upgrade_lock<boost::shared_mutex> list_read_lock(_list_mutex);
        _latest = _cache_ptr->get_blk<cache_node>(0);
        for (int32_t i = 1; i < _cache_count; i++) {
            update_list(_cache_ptr->get_blk<cache_node>(i), list_read_lock);
        }
        FUNCTION_DEBUG_LOG(logger, "LRU LIST=" << to_string());
    }

    cache_node *lru_indexable_list::get_node(int32_t index, bool decrypt,
                                             boost::upgrade_lock<boost::shared_mutex> &cache_shared_lock) {
        boost::upgrade_lock<boost::shared_mutex> index_shared_lock(_index_mutex);

        lru_index &_pair = _cache_index[index];
        get_cache_index(_pair, index, decrypt, cache_shared_lock, index_shared_lock);

        if (_pair.encrypt->is_uninited()) {
            _pair.encrypt->blk2cache();
        }

        if (decrypt) {
            if (_pair.decrypt->is_uninited()) {
                if (_pair.encrypt->is_updated()) {
                    _pair.encrypt->cache2blk();
                }
                _pair.decrypt->decrypt();
            }
        }

        if (!decrypt && (_pair.decrypt != nullptr) && _pair.decrypt->is_updated()) {
            assert(_pair.decrypt->is_me(index));
            _pair.decrypt->encrypt();
        }

        return (decrypt ? _pair.decrypt : _pair.encrypt);
    }

    void lru_indexable_list::get_cache_index(lru_index &_pair, int blk_index, bool decrypt,
                                             boost::upgrade_lock<boost::shared_mutex> &cache_shared_lock,
                                             boost::upgrade_lock<boost::shared_mutex> &index_shared_lock) {

        auto use_exist_first = [&](cache_node *_node, bool _type, bool _require) -> void {
            if (_require && (_node != nullptr) && _node->is_me(blk_index) && (_type == _node->is_decrypted())) {
                boost::upgrade_lock<boost::shared_mutex> list_shared_lock(_list_mutex);
                update_list(_node, list_shared_lock);
            }
        };

        auto otherwise_use_oldest = [&](cache_node *&_node, bool _require) -> bool {
            if (!_require) {
                if ((_node != nullptr) && !_node->is_me(blk_index)) {
                    boost::upgrade_to_unique_lock<boost::shared_mutex> index_write_lock(index_shared_lock);
                    _node = nullptr;/* not required & not me, set to nullptr.*/
                }
                return false;
            }

            bool _update = false;
            if ((_node == nullptr) || !_node->is_me(blk_index)) {
                boost::upgrade_to_unique_lock<boost::shared_mutex> index_write_lock(index_shared_lock);
                _node = _latest->prev;
                _update = true;
            }
            if (_update && _node->is_updated()) {
                _node->flush();
            }
            if (_require || _update) {
                boost::upgrade_lock<boost::shared_mutex> list_shared_lock(_list_mutex);
                update_list(_node, list_shared_lock);
            }
            return _update;
        };

        use_exist_first(_pair.decrypt, true, decrypt);
        use_exist_first(_pair.encrypt, false, true);
        FUNCTION_DEBUG_LOG(logger, "LRU LIST=" << to_string());

        bool update_decrypt_index = otherwise_use_oldest(_pair.decrypt, decrypt);
        bool update_encrypt_index = otherwise_use_oldest(_pair.encrypt, true);

        if (update_decrypt_index || update_encrypt_index) {
            boost::upgrade_to_unique_lock<boost::shared_mutex> write_lock(cache_shared_lock);
            if (_pair.decrypt) {
                _pair.decrypt->reset_index(blk_index, _pair.encrypt);
            }
            if (update_encrypt_index) {
                _pair.encrypt->reset_index(blk_index, nullptr);
            }
        }
        FUNCTION_DEBUG_LOG(logger, "LRU LIST=" << to_string());
    }


    void lru_indexable_list::update_list(cache_node *node, boost::upgrade_lock<boost::shared_mutex> &list_shared_lock) {
        assert(node);
        assert(_latest);
        assert(_latest->prev);
        assert(_latest->next);
        if (node == _latest) {
            return;
        }
        boost::upgrade_to_unique_lock<boost::shared_mutex> write_lock(list_shared_lock);

        /*unlink current node.*/
        node->next->prev = node->prev;
        node->prev->next = node->next;

        /*link current node after the prev.*/
        _latest->prev->next = node;
        node->prev = _latest->prev;

        /*link current node before the latest.*/
        _latest->prev = node;
        node->next = _latest;

        /*make current node to be the latest.*/
        _latest = node;
    }

}