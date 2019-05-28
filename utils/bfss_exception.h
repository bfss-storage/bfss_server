//
// Created by han on 19-2-7.
//

#ifndef BFSS_ALL_EXCEPTION_H
#define BFSS_ALL_EXCEPTION_H

#include <string>
#include <exception>
#include <tuple>
#include <iostream>

#include <log4cxx/logger.h>
#include <log4cxx/helpers/exception.h>
#include <log4cxx/helpers/messagebuffer.h>
#include <bfss_result_types.h>
#include <client/linux/handler/exception_handler.h>

using namespace log4cxx;
using namespace log4cxx::helpers;
extern LoggerPtr logger;


#define EXCEPTION_ERROR_LOG(logger,e) \
    LOG4CXX_ERROR(logger, "EXCEPTION: what(" << e.what() << ")")

#define FUNCTION_ENTRY_DEBUG_LOG(logger,...) \
    LOG4CXX_DEBUG(logger, "ENTRY: " << __FUNCTION__ << std::make_tuple(__VA_ARGS__))

#define FUNCTION_EXIT_DEBUG_LOG(logger,...) \
    LOG4CXX_DEBUG(logger, "EXIT: " << __FUNCTION__ << std::make_tuple(__VA_ARGS__) << " - success")

#define FUNCTION_WARN_LOG(logger,str) \
    LOG4CXX_WARN(logger, str)

#define FUNCTION_DEBUG_LOG(logger,str) \
    LOG4CXX_DEBUG(logger, str)

#define THROW_BFSS_EXCEPTION(t,x)       throw bfss::BFSS_Exception(t, x,  __FILE__ ,__LINE__)

#define CATCH_BFSS_EXCEPTION_RETURN()   \
    catch (bfss::BFSS_Exception &e) { FUNCTION_WARN_LOG(logger, e.what()); return (::bfss::BFSS_RESULT::type)e.type(); } \
    catch (std::exception &e)       { FUNCTION_WARN_LOG(logger, e.what()); return ::bfss::BFSS_RESULT::BFSS_UNKNOWN_ERROR; }


#define CATCH_BFSS_EXCEPTION_RESULT(r)  \
    catch (bfss::BFSS_Exception &e) {   \
        LOG4CXX_WARN(logger, e.what()); \
        r.Result =  (::bfss::BFSS_RESULT::type)e.type();\
    } catch (std::exception &e) {       \
        LOG4CXX_WARN(logger, e.what()); \
        r.Result = ::bfss::BFSS_RESULT::BFSS_UNKNOWN_ERROR; \
    }



namespace std{
    inline string make_tuple(){
        return "()";
    }
}

namespace log4cxx::helpers{
    template<typename Type, unsigned N, unsigned Last>
    struct tuple_printer {
        static void print(std::ostream& out, const Type& value) {
            out << std::get<N>(value) << ", ";
            tuple_printer<Type, N + 1, Last>::print(out, value);
        }
    };

    template<typename Type, unsigned N>
    struct tuple_printer<Type, N, N> {
        static void print(std::ostream& out, const Type& value) {
            out << std::get<N>(value);
        }
    };

    template<typename... Types>
    inline std::basic_ostream<char>& operator<<(CharMessageBuffer& os, const std::tuple<Types...>& value) {
        os <<"(";
        tuple_printer<std::tuple<Types...>, 0, sizeof...(Types) - 1>::print(os, value);
        os << ")";
        return ((std::basic_ostream<char>&) os);
    }
}


namespace bfss {

    class BFSS_Exception : public std::exception {
    public:

        BFSS_Exception(bfss::BFSS_RESULT::type type, const std::string &err, const char *file = __FILE__, int line = __LINE__) : std::exception() {
            std::ostringstream oss;
            oss << "\n[BFSS EXCEPTION] " << file << " (line:" << line << ")  TYPE[" << type << "]: " << err;
            _what = oss.str();
            _type = type;
        }

        BFSS_Exception() = delete;

        virtual const char *
        what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_USE_NOEXCEPT {
            return _what.c_str();
        }

        bfss::BFSS_RESULT::type type() {
            return _type;
        }

    protected:
        std::string _what;
        bfss::BFSS_RESULT::type _type;
    };

    inline void bfss_try(std::function<void()> __try_execute,std::function<void(bfss::BFSS_RESULT::type, const char *)> _catch_execute){
        try{
            __try_execute();
        }catch (BFSS_Exception &e){
            _catch_execute(e.type(),e.what());
        }catch (std::exception &e) {
            _catch_execute(BFSS_RESULT::BFSS_UNKNOWN_ERROR, e.what());
        }
    }
}
#endif //BFSS_ALL_EXCEPTION_H
