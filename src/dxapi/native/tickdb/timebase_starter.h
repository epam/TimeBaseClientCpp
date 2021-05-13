#include "tickdb/common.h"
#include <dxapi/dxapi.h>


namespace DxApiImpl {
    class TimebaseRemoteStarter {
    public:
        static bool isRunning(DxApi::TickDb  * db);
        static bool isRunning(const char * host);
        static bool start(const char * host, const char * dir);
        static void stop(const char * host);

        static std::string getDeltixHome();
        static std::string getTmpHome();
    };


    class TimebaseInstance {
    public:
        bool isRunning() const;


        bool isExternal() const
        {
            return external_;
        }


        bool start() const;
        void stop() const;


        TimebaseInstance(const char *host, const char *dir, bool external = false);
        TimebaseInstance(const char *host);

        TimebaseInstance(TimebaseInstance &&other) : host(std::move(other.host)), dir(std::move(other.dir)), external_(other.external_)
        {
            other.host.clear();
            other.dir.clear();
        }

        ~TimebaseInstance();

    protected:
        std::string host, dir;
        bool external_;
    };
}
