//
// Created by root on 19-2-8.
//

#include "bfss_cache.h"
#include "bfss_utils.h"
#include "bfss_mongo_utils.h"
#include <openssl/rand.h>
#include <openssl/err.h>
using namespace bfss;

namespace BFSS_SN {

    int32_t cache_manager::init(const std::string &volume_name, int64_t max_block_size, int64_t max_cache_size,
                                keys_manager &key_mgr) {

        if (!_s_blk_device.open(volume_name.c_str())) {
            THROW_BFSS_EXCEPTION(BFSS_RESULT::BFSS_DATA_WRITE_INCOMPLETE, " can NOT open block device" + volume_name);
        }

        int32_t _blk_count = _s_blk_device.blocks();
        if ((int64_t(_blk_count) * blk_size) > max_block_size) {
            _blk_count = int(max_block_size / blk_size);
        }

        _lru_list.init(_blk_count, max_cache_size, key_mgr);

        _cache___async_thread = std::make_shared<std::thread>(std::bind(&cache_manager::flush_async, this));
        _cache_timeout_thread = std::make_shared<std::thread>(std::bind(&cache_manager::flush_timeout, this));
        _initd = true;
        return _blk_count;
    }

    int32_t cache_manager::write(const int32_t index, const int32_t offset, const int32_t size, const char *data) {
        int32_t _write_size = 0;
        _lru_list.access(index, true, /*action*/[&](cache_node *node) {
            _write_size = node->write(index, offset, size, data);
        });
        return _write_size;
    }

    int32_t cache_manager::read(const int32_t index, const int32_t offset, const int32_t size, std::string &data,
                                bool decrypt) {
        int32_t _read_size = 0;
        _lru_list.access(index, decrypt, /*action*/[&](cache_node *node) {
            _read_size = node->read(index, offset, size, data, decrypt);
        });
        return _read_size;
    }

    void cache_manager::flush_async() {
        while (1) {
            while (_spsc_queue.read_available() > 0) {
                int32_t index = -1;
                if (_spsc_queue.pop(index)) {
                    if (index != -1) {
                        FUNCTION_DEBUG_LOG(logger, "_spsc_queue.pop(" << index << ") success.");
                        _lru_list.access(index, true, /*action*/[&](cache_node *node) {
                            if (node->is_me(index) && node->is_updated()) {
                                node->flush();
                            }
                        });
                        _lru_list.access(index, false, /*action*/[&](cache_node *node) {
                            if (node->is_me(index) && node->is_updated()) {
                                node->flush();
                            }
                        });
                    }
                }
            }

            std::unique_lock lock(_condition_mutex);
            if (std::cv_status::no_timeout == _condition.wait_for(lock, std::chrono::milliseconds(1 * 1000)) || !_initd) {
                break;
            }
        }
    }

    void cache_manager::flush_timeout() {
        while (1) {

            int64_t _now = time(nullptr);
            int32_t _timeout = 10;

            _lru_list.traverse([&](cache_node *node) {
                if (node->is_updated() && node->is_timeout(_now, _timeout)) {
                    _spsc_queue.push(node->get_blk_index());
                    FUNCTION_DEBUG_LOG(logger, "_spsc_queue.push(" << node->to_string() << ") .");
                    return (_spsc_queue.write_available() > 0);
                }
                return false; // stop..
            });

            std::unique_lock lock(_condition_mutex);
            if (std::cv_status::no_timeout == _condition.wait_for(lock, std::chrono::milliseconds(1 * 1000)) || !_initd) {
                break;
            }
        }
    }

    void cache_manager::flush_and_stop() {
        FUNCTION_ENTRY_DEBUG_LOG(logger);
        if (_initd) {
            _condition.notify_all();
            _initd = false;
            _cache_timeout_thread->join();
            _cache___async_thread->join();

            _lru_list.traverse([&](cache_node *node) {
                if (node->is_updated()) {
                    node->flush();
                }
                return true;
            });
        }
        _cache___async_thread = nullptr;
        _cache_timeout_thread = nullptr;
    }
}

