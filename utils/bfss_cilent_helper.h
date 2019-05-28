//
// Created by han on 19-3-10.
//

#ifndef BFSS_ALL_BFSS_CILENT_HELPER_H
#define BFSS_ALL_BFSS_CILENT_HELPER_H
#include <chrono>
#include <memory>
#include <tuple>

using namespace ::apache::thrift::protocol;

template<typename cilent_t>
class bfss_cilent_helper : public cilent_t{
    typedef std::chrono::time_point<std::chrono::system_clock,std::chrono::milliseconds> _milliseconds;
public:

    template<typename ..._args>
    bfss_cilent_helper(_args... args): cilent_t(args...){
        _transport = std::get<0>(std::make_tuple(args...))->getTransport();
        _last = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

    }
public:
    cilent_t *get_client(){
        auto _now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        if((_last.time_since_epoch().count() + 1000*120) > _now.time_since_epoch().count()){
            if(_transport->isOpen()){
                _transport->close();
            }
        }
        if(!_transport->isOpen()){
            _transport->open();
        }
        _last = _now;
        return static_cast<cilent_t*>(this);
    }
    template<typename _fun_t,typename ..._args>
    auto try_call_rpc_fun(_fun_t _fun, _args&... args){
        bool _reopen = false;
        while (true){
            try {
                return (static_cast<cilent_t*>(this)->*_fun)(args...);
            }catch (const std::exception &e){
                if(!_reopen){
                    _reopen = true;
                    if(_transport->isOpen()){
                        _transport->close();
                    }
                    _transport->open();
                    continue;
                }
                else{
                    throw e;
                }
            }
        }
    }
private:
    _milliseconds _last;
    std::shared_ptr<TTransport> _transport;
};

template<typename _o_t,typename _fun_t,typename ..._args>
inline auto try_call_t(_o_t *_o,_fun_t _fun, _args&... args){
    return static_cast<bfss_cilent_helper<_o_t>*>(_o)->try_call_rpc_fun(_fun,args...);
}

#endif //BFSS_ALL_BFSS_CILENT_HELPER_H
