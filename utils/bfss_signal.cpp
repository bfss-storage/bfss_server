//
// Created by han on 19-3-6.
//

#include <unistd.h>
#include <signal.h>
#include <assert.h>

#include "bfss_signal.h"
#include "bfss_exception.h"

class bfss::bfss_signal *_this = nullptr;
namespace bfss{
    class bfss_signal_ : public bfss::bfss_signal{
    public:
        std::map<int,std::list<std::function<bool()>>>& get_map(){ return _signal_map;}
        std::vector<struct sigaction>& get_vector(){ return _signal_vector;};
    };
    void _sig_handle_(int _sig, siginfo_t * siginfo, void *p){
        auto _map = static_cast<bfss_signal_*>(_this)->get_map();
        auto _it = _map.find(_sig);
        assert(_it != _map.end());
        signal(_sig,SIG_IGN);
        auto & _org_sig = static_cast<bfss_signal_*>(_this)->get_vector();
        bool _call_sys = false;
        for (auto _i:_it->second){
            if(_i()){
                _call_sys = true;
            }
        }
        if(_call_sys){
            if(_org_sig[_sig].sa_sigaction)
                _org_sig[_sig].sa_sigaction(_sig,siginfo,p);
        }
    }
}

void bfss::bfss_signal::register_handle(int _signal,std::function<bool()> &&handle){
    assert(__SIGRTMAX > _signal);
    _signal_map[_signal].push_back(std::move(handle));

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    if(sigaction(_signal, nullptr, &_signal_vector[_signal]) == -1){
        return;
    }
    sigaddset(&sa.sa_mask, _signal);

    sa.sa_sigaction = _sig_handle_;
    sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
    sigaction(_signal, &sa, nullptr);

}
void bfss::bfss_signal::__un_init(){
    assert(_this == this);
    for(auto _i:static_cast<bfss_signal_*>(_this)->get_map()){
        if (sigaction(_i.first, &_signal_vector[_i.first], nullptr) == -1) {
            signal(_i.first, SIG_DFL);
        }
    }
    _this = nullptr;
}
void bfss::bfss_signal::__init() {
    assert(_this == nullptr);
    _this = this;
    _signal_vector.resize(__SIGRTMAX);
}
