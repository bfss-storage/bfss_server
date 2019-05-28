//
// Created by root on 19-2-8.
//

#include "bfss_regm.h"
#include "bfss_exception.h"
#include <log4cxx/propertyconfigurator.h>

using namespace log4cxx;
using namespace log4cxx::helpers;
using namespace std;

libconfig::Config config;
LoggerPtr logger(Logger::getLogger("BFSS_REGMD"));

bool MiniDCallBack(const google_breakpad::MinidumpDescriptor& descriptor,
                         void* context,
                         bool succeeded) {
    LOG4CXX_WARN(logger, "MINI-DUMP path: " << descriptor.path());
    return succeeded;
}

int main(int argc, char **argv) {
    google_breakpad::MinidumpDescriptor descriptor("/tmp");
    google_breakpad::ExceptionHandler eh(descriptor, NULL, MiniDCallBack, NULL, true, -1);
    //BasicConfigurator::configure();
    PropertyConfigurator::configure("./log4cxx.bfss_regm.properties");

    LOG4CXX_INFO(logger, "Entering application.");
    try{
        config.readFile("./bfss_regm.cfg");
    } catch(libconfig::FileIOException &e){
        EXCEPTION_ERROR_LOG(logger, e);
        return -1;
    }
    catch(libconfig::ParseException &e){
        EXCEPTION_ERROR_LOG(logger, e);
        return -1;
    }catch(std::exception &e){
        EXCEPTION_ERROR_LOG(logger, e);
        return -1;
    }
    try {
        int result = BFSS_REGMD::StartServer(config);
        LOG4CXX_INFO(logger, "Exit application." );
        return result;
    }catch(std::exception &e){
        EXCEPTION_ERROR_LOG(logger,e);
        return -1;
    }
}
