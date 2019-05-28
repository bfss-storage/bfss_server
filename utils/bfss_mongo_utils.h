//
// Created by han on 19-2-12.
//

#ifndef BFSS_ALL_BFSS_MONGO_UTILS_H
#define BFSS_ALL_BFSS_MONGO_UTILS_H

#include <bsoncxx/builder/basic/document.hpp>

#include <bfss_utils.h>


namespace bfss{

    template <typename _value_t>
    _value_t &_get_mongo_value(const ::bsoncxx::document::view::iterator &_it,_value_t &_value);

#define __get_mongo_value(T,GET_XXX)\
    template <>\
    inline T &_get_mongo_value<T>(const ::bsoncxx::document::view::iterator &_it,T &_value){\
        return _value = const_cast<::bsoncxx::document::view::iterator&>(_it)->GET_XXX;\
    }

    __get_mongo_value(int32_t,get_int32())
    __get_mongo_value(int64_t,get_int64())
    __get_mongo_value(bool,get_bool())
    __get_mongo_value(double,get_double().value)
    __get_mongo_value(std::string,get_utf8().value.to_string())
    __get_mongo_value(std::chrono::milliseconds,get_date().value)
    __get_mongo_value(::bsoncxx::types::b_binary,get_binary())
    __get_mongo_value(::bsoncxx::types::b_dbpointer,get_dbpointer())
    __get_mongo_value(::bsoncxx::types::b_decimal128,get_decimal128())
    __get_mongo_value(::bsoncxx::types::b_document,get_document())

#undef __get_mongo_value


    template <typename _value_t>
    inline bool get_mongo_document_value(const ::mongocxx::stdx::optional<::bsoncxx::document::value> &_doc,
                                         const char *_key, _value_t &_value){
        ::bsoncxx::document::view::iterator it;
        if(has_lockup(_doc.value().view(),_key,it)){
            _get_mongo_value<_value_t>(it,_value);
            return true;
        }
        return false;
    }
    template <typename _value_t>
    inline bool get_mongo_document_value(const ::bsoncxx::document::view_or_value &_view,
                                         const char *_key, _value_t &_value){
        ::bsoncxx::document::view::iterator it;
        if(has_lockup(_view.view(),_key,it)){
            _get_mongo_value<_value_t>(it,_value);
            return true;
        }
        return false;
    }
    template<typename T>
    inline T& bin2t(const ::bsoncxx::types::b_binary &_binary,T & _t){
        return _t = T(reinterpret_cast<const char*>(_binary.bytes),_binary.size);
    }
#define get_mongo_value_value_t(_value,_key_name,get_value_t)\
        _value.view()[_key_name].get_value_t

#define get_mongo_document_value_t(_document,_key_name,get_value_t)\
        get_mongo_value_value_t(_document.value(),_key_name,get_value_t)
}

#endif //BFSS_ALL_BFSS_MONGO_UTILS_H
