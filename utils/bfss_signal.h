//
// Created by han on 19-3-6.
//

#ifndef BFSS_ALL_BFSS_SIGNAL_H
#define BFSS_ALL_BFSS_SIGNAL_H

#include <map>
#include <list>
#include <functional>
#include <vector>


namespace bfss{
    class bfss_signal {
    public:
        bfss_signal(){
            __init();
        }
        ~bfss_signal(){
            __un_init();
        }

        void register_handle(int _signal,std::function<bool()> &&handle);

    protected:
        void __init();
        void __un_init();
        std::map<int,std::list<std::function<bool()>>> _signal_map;
        std::vector<struct sigaction> _signal_vector;
    };
}


#endif //BFSS_ALL_BFSS_SIGNAL_H
