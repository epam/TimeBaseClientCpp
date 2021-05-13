#include "timebase_starter.h"
#include "tickdb/http/tickdb_http.h"

#include <thread>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
static int _mkdir(const char *dir)
{
    return mkdir(dir, (S_IFDIR | 0666));
}

#define _getcwd     getcwd
#define _popen      popen
#define _pclose     pclose

#endif


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;

#define DELTIX_HOME "DELTIX_HOME"
#define START_TIMEOUT_MS 100000



std::string TimebaseRemoteStarter::getDeltixHome()
{
    char dir[0x2000];
    string out;
    auto dh = getenv(DELTIX_HOME);
    if (NULL != dh && false) {
        out.assign(dh);
    }
    else {
        dh = _getcwd(dir, sizeof(dir));
        if (NULL == dh) {
            dh = ".";
        }

        out.assign(dh);
        out.append("/../../..");
    }

    return out;
}


std::string TimebaseRemoteStarter::getTmpHome()
{
    string out = getDeltixHome();
    out.append("/temp");
    return out;
}


bool TimebaseRemoteStarter::isRunning(DxApi::TickDb *db)
{
    std::string dummy;
    dx_assert(NULL != db, "db can't be NULL");

    try {
        impl(db)->executeRequest(dummy, "GET", "/", NULL, NULL, true);
        delete db;
        return dummy.size() != 0;
    }
    catch (const std::exception &) {
    }

    return false;
}


bool TimebaseRemoteStarter::isRunning(const char *uri)
{
    try {
        auto db = TickDbImpl::createFromUrl(uri);

        if (NULL != db) {
            return isRunning(db);
        }
    }
    catch (const std::exception &) {
    }
    
    return false;
}


bool TimebaseRemoteStarter::start(const char *host, const char *dir)
{
    printf("Starting %s @ HOME: %s\n", host, dir);
    DxApiImpl::Uri uri;
    if (!uri.parse(host)) {
        return false;
    }

    // Check URL again
    auto db = TickDbImpl::createFromUrl(host);
    if (NULL != db) {
        delete db;
    }

    string cmd;
    string dxHome = TimebaseRemoteStarter::getDeltixHome();

    format_string(&cmd, "%s/jre/bin/java.exe -server -Ddeltix.home=\"%s\" -Xmx1024M -jar \"%s/bin/runjava.jar\" deltix.qsrv.comm.cat.TomcatCmd -home \"%s\" -port %u",
        dxHome.c_str(), dxHome.c_str(), dxHome.c_str(), dir, uri.port);

    puts(cmd.c_str());

    FILE * f = _popen(cmd.c_str(), "rt");
    if (NULL == f) {
        return false;
    }

    try {
        int64_t remaining = START_TIMEOUT_MS;
        while (remaining > 0) {
            if (isRunning(host)) {
                return true;
            }

            this_thread::sleep_for(chrono::milliseconds(50));
            remaining -= 50;
        }

        if (NULL != f) {
            _pclose(f);
        }
    }

    catch (...) {
        if (NULL != f) {
            _pclose(f);
        }

        throw;
    }

    return false;
}


void TimebaseRemoteStarter::stop(const char *uri)
{
    std::string dummy;
    try {
        auto db = TickDbImpl::createFromUrl(uri);

        //printf("Stopping: %s\n", uri);
        if (NULL != db) {
            try {
                while (1) {
                    impl(db)->executeRequest(dummy, "GET", "/shutdown...", NULL, NULL, true);
                    this_thread::sleep_for(chrono::milliseconds(20));
                }
            }

            catch (std::exception &) {}
            delete db;
        }
    }

    catch (std::exception &) {
        // Failure to create unconnected instance is ignored in this particulat case
    }
}


TimebaseInstance::TimebaseInstance(const char *hostArg, const char *dirArg, bool external) : host(hostArg), dir(dirArg), external_(external)
{
}


TimebaseInstance::TimebaseInstance(const char *host) : TimebaseInstance(host, "", true)
{
}


bool TimebaseInstance::start() const
{
    if (isExternal()) {
        return isRunning();
    }

    return TimebaseRemoteStarter::start(host.c_str(), dir.c_str());
}


void TimebaseInstance::stop() const
{
    if (!isExternal())
        TimebaseRemoteStarter::stop(host.c_str());
}


bool TimebaseInstance::isRunning() const
{
    return TimebaseRemoteStarter::isRunning(host.c_str());
}


TimebaseInstance::~TimebaseInstance()
{
    stop();
}