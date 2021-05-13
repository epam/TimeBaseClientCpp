#include "tickdb/common.h"
#include "util/critical_section.h"
#include "util/concurrent_object_set.h"
#include <exception>
#include <ctime>
#include <algorithm>
#include <thread>
#include <chrono>
#include <unordered_set>
#include "janitor.h"


#define LOGHDR "Watcher: "
#define DBGLOG_VERBOSE DBGLOG


using namespace std;
//using namespace DxApi;
using namespace DxApiImpl;
//using namespace DxApiImpl::TDB;
using namespace dx_thread;


DxApiImpl::Janitor janitor;


INLINE bool Janitor::isRunning() const
{
    return isRunning_;
}


void Janitor::stopped()
{
    //yield_lock section(this->threadObjSection_);

    auto thread = this->thread_;
    this->thread_ = NULL;
    if (NULL != thread) {
        thread->detach();
        delete thread;
    }

    isStopped_ = true;
    isRunning_ = false;
}


void Janitor::stop()
{
    DBGLOG_VERBOSE(LOGHDR " stopping");
    shouldStop_ = true;

    if (isRunning() != (NULL != thread_)) {
        DBGLOG(LOGHDR ".sessionHandler.stop(): ERROR: isRunning() is %d, but thread is %p !!", (int)isRunning(), thread_);
    }

    DBGLOG_VERBOSE(LOGHDR ".sessionHandler.stop(): FINISHED");
}



void Janitor::threadProcImpl()
{
    DBGLOG(LOGHDR "thread started");

    //static_assert(PING_PERIOD_NS > IDLE_YIELD_DELAY_NS, "Ping period must be greater than thread yield/sleep period");

    //auto lastDataTimestamp = (int64_t)time_ns();
    //auto lastLoggedTimestamp = lastDataTimestamp;
    //auto lastPingTimestamp = lastDataTimestamp;
    //int64_t logPeriod = INT64_C(5000000000);
    //int64_t pingPeriod = PING_PERIOD_NS;

    while (!shouldStop_) {
        int64 t = time_ns();

        //if (OK == result) {
        //    lastPingTimestamp = lastDataTimestamp = lastLoggedTimestamp = time_ns();
        //}
        //else {
        //    /*if (t - lastDataTimestamp > IDLE_UNLOCK_TIMEOUT_NS && knownStreamDefs.size() != 0) {
        //    knownStreamDefs.clear();
        //    }*/

        //    if (t - lastDataTimestamp > IDLE_YIELD_DELAY_NS) {
        //        if (t - lastLoggedTimestamp > logPeriod) {
        //            lastLoggedTimestamp += logPeriod;
        //            DBGLOG_VERBOSE(LOGHDR ": idle", ID);
        //        }

                // Ping disabled for now
#if 0
                if (t - lastPingTimestamp > pingPeriod) {
                    lastPingTimestamp += pingPeriod;
                    requestGetStream("__PING_u5u7clwetyjui__");
                }
#endif

                //this_thread::yield();
            //}

    }

    DBGLOG_VERBOSE(LOGHDR "thread stopped");
}



void Janitor::threadProcExceptionWrapper()
{
    try {
        threadProcImpl();
        //dx_thread::atomic_exchange(isStopped_, true);
    }
    catch (const std::exception &e) {
        DBGLOG(LOGHDR ".threadProc(): ERR: Exception caught: %s", e.what());
    }
    catch (...) {
        DBGLOG(LOGHDR ".threadProc(): ERR: System exception caught!");
    }
}


void Janitor::threadProc()
{
    isRunning_ = true;
    threadProcExceptionWrapper();
    stopped();
}


void Janitor::threadProcStatic(Janitor &self)
{
    try {
        self.threadProc();
    }
    catch (...) {
        dx_assert(!"Uncaught exception in SessionHandler threadproc!!!", "ERR: Uncaught exception in SessionHandler threadproc!!!");
    }
}



void Janitor::init()
{
    isRunning_ = shouldStop_ = false;

    // Ok, start worker thread
    thread_ = new std::thread(threadProcStatic, std::ref(*this));
    while (!isRunning_ && !isStopped_) {
        this_thread::yield();
    }
}


void Janitor::dispose()
{

}

