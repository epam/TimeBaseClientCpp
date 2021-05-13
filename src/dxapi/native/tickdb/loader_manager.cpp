#include <thread>
#include <chrono>
#include <unordered_set>

#include "http/tickdb_http.h"
#include "http/tickloader_http.h"
#include "loader_manager.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace dx_thread;

#define LOADER_CLOSE_RESPONSE_TIMEOUT_MS 2000


#if VERBOSE_TICKDB_MANAGER_THREAD >= 1
#define DBGLOG_VERBOSE DBGLOG
#else
#define DBGLOG_VERBOSE (void)
#endif

#if VERBOSE_TICKDB_MANAGER_THREAD >= 2
#else
#endif


#define LOGHDR "%s"
#define ID (this->db_.textId())


#define READ_LOCK srw_read section(thisLock_);
#define WRITE_LOCK srw_write section(thisLock_);

// Returns true if this instance of the manager is running
INLINE bool LoaderManager::isRunning() const
{
    return isRunning_;
}

// Add loader to the manager. The manager will poll loader's input stream (socket).
void LoaderManager::add(TickLoaderImpl * loader)
{
    loaders_.add(loader);
}


TickLoaderImpl * LoaderManager::find(TickLoaderImpl * x)
{
    return NULL != x ? loaders_.find(x) : x;
}


using namespace TickLoaderNs;

// Remove loader from the list, delete all its listeners
// Will wait until the loader's input stream is closed if the waitForCompletion flag is true
bool LoaderManager::remove(TickLoaderImpl * loader, bool waitForCompletion)
{
    { WRITE_LOCK
        auto l = loaders_.find(loader);
        if (NULL == l || being_removed_.find(l)) {
            return false;
        }

        if (!waitForCompletion) {
            //assert(!loader->dbgProcessingServerResponse_);
            DBGLOG(LOGHDR ".removeLoader(%s) loaders_.remove(loader) start", ID, loader->textId());
            loaders_.remove(loader);
            DBGLOG(LOGHDR ".removeLoader(%s) loaders_.remove(loader) end", ID, loader->textId());
            //assert(NULL == loaders_.find(loader));
            return true;
        }

        // To avoid double removal
        being_removed_.add(loader);
    }

    // We can get here only if waitForCompletion == true

    assert(waitForCompletion);

    try {
        if (waitForCompletion) {
            int remaining = LOADER_CLOSE_RESPONSE_TIMEOUT_MS;
            for (; remaining > 0; --remaining) {
                if (loader->interruption_ > Interruption::STOPPING) {
                    DBGLOG(LOGHDR ".removeLoader(%s): waited until interruption flag", ID, loader->textId());
                    break;
                }

                this_thread::sleep_for(chrono::milliseconds(1));
            }

            if (remaining <= 0) {
                DBGLOG(LOGHDR ".removeLoader(%s) Timeout! No proper response from the server.", ID, loader->textId());
            }
        }

        { WRITE_LOCK
            being_removed_.remove(loader);
            loaders_.remove(loader);
        }
    }
    catch (...) {
        DBGLOG(LOGHDR ".removeLoader(%s): Exception occured", ID, loader->textId());
        { WRITE_LOCK
            being_removed_.remove(loader);
            loaders_.remove(loader);
        }

        throw;
    }

    return true;
}


// Add  listener for running loader
void LoaderManager::addListener(TickLoaderImpl * loader, LoaderListenerCallback callback)
{
    srw_write section(thisLock_);
    // TODO: Still no time :(
}


// remove listener for a loader
void LoaderManager::removeListener(TickLoaderImpl * loader, LoaderListenerCallback callback)
{
    srw_write section(thisLock_);
}


/*
bool LoaderManager::start()
{
    if (isRunning()) {
        return true;
    }
}
*/

size_t LoaderManager::loadersCount()
{
    return loaders_.size();
}


void LoaderManager::stop()
{
    DBGLOG_VERBOSE(LOGHDR ".loaderManager.stop(): STARTED", ID);
    srw_write section(thisLock_);
    shouldStop_ = true;
    DBGLOG_VERBOSE(LOGHDR ".loaderManager.stop(): isRunning: %d, thread_=%p", ID, (int)isRunning(), thread_);
    if (!isRunning() && NULL == thread_) {
        DBGLOG_VERBOSE(LOGHDR ".loaderManager.stop(): STOPPED", ID);
        return;
    }
    
    if (isRunning() != (NULL != thread_)) {
        DBGLOG(LOGHDR ".loaderManager.stop(): ERR: isRunning() is %d, but thread is %p !!", ID, (int)isRunning(), thread_);
    }

    if (NULL != thread_) {
        thread_->join();
        delete thread_;
        thread_ = NULL;
        DBGLOG_VERBOSE(LOGHDR ".loaderManager.stop(): STOPPED", ID);
    }
}


bool LoaderManager::start()
{
    srw_write section(thisLock_);
    if (isRunning() || NULL != thread_) {
        THROW_DBGLOG(LOGHDR ".loaderManager.start(): Already started", ID);
    }

    thread_ = new std::thread(threadProcStatic, std::ref(*this));
    isRunning_ = true;
    return true;
}


LoaderManager::LoaderManager(TickDbImpl &db) : db_(db), thread_(NULL), isRunning_(false), shouldStop_(false)
{
}


LoaderManager::~LoaderManager()
{
    DBGLOG(LOGHDR ".~loaderManager(): destructor started", ID);

    if (NULL != thread_) {
        DBGLOG(".~loaderManager(): ERR: thread is still not NULL!");
        // Will terminate thread
        delete thread_;
    }

    DBGLOG(LOGHDR ".~loaderManager(): deleted", ID);
}


void LoaderManager::threadProcStatic(LoaderManager &self)
{
    try {
        self.threadProc();
    }
    catch (...) {
        DBGLOG("LoaderManager: ERR: uncaught exception!!!");
        assert(!"Uncaught exception in LoaderManager!!!");
    }
}


void LoaderManager::process(TickLoaderImpl * loader)
{
    try {
        srw_read section(loaders_.lock());
        if (loaders_.find(loader)) {
            loader->processResponse();

#if LOADER_FLUSH_TIMER_ENABLED
            // If not immediately flushed
            if (loader->minChunkSize_ > 0) {
                if (loader->flushable_.test_and_set()) {
                    if () {
                        bool shouldBeFlushed = loader->willBeFlushedByManager_.test_and_set(memory_order_acq_rel);
                        if (shouldBeFlushed) {
                            loader->beingFlushed_.test_and_set(memory_order_acq_rel);

                            loader->willBeFlushedByManager_.clear(memory_order_release);
                            loader->beingFlushed_.clear(memory_order_release);
                        }
                    }
                }
                
            }
#endif // #if LOADER_FLUSH_TIMER_ENABLED
        }
    }
    catch (const std::exception &e) {
        DBGLOG(LOGHDR ".loaderManager: ERR: loader %s threw an exception: %s!", ID, loader->textId(), e.what());
        // We can't allow exceptions in this thread
        // TODO: We will ignore this in Release build, but throw in debug build
        assert(!"Exception while processing input stream of a loader");
    }

    catch (...) {
        DBGLOG(LOGHDR ".loaderManager: ERR: loader %s threw an exception!", ID, loader->textId());
        // We can't allow exceptions in this thread
        // TODO: We will ignore this in Release build, but throw in debug build
        assert(!"Exception while processing input stream of a loader");
    }
}


/**
 * TODO: Flushing TickLoader
 *
 * If Loader sees lock flag, he tries to flush himself, but under exclusive lock(if necessary), and later resets the flag.
 * Flusher thread watchdog timer detects loader that does not do any work and has data in the buffer. It sets "will flush" flag, waits a bit more(to synch cache), and then does flush under exclusive lock
 * while udner lock it also resets all flags, obviously.
 *
 * 
 */
void LoaderManager::processAll()
{
    concurrent_ptr_set_iter iter(loaders_);
    TickLoaderImpl * i;

    while (NULL != (i = (TickLoaderImpl *)iter.next())) {
        process(i);
        this_thread::yield();
    }
}


void LoaderManager::threadProc()
{
    isRunning_ = true;
    this_thread::sleep_for(chrono::milliseconds(1));
    DBGLOG(LOGHDR ": loaderManager started!", ID);
    while (!shouldStop_) {
        // TODO: Later should switch to select()
        processAll();
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    unsigned n = (unsigned)loaders_.size();

    if (0 == n) {
        DBGLOG(LOGHDR ".loaderManager: thread finished!", ID);
    }
    else {
        DBGLOG(LOGHDR ".loaderManager: ERR: thread finished with %u orphaned loaders!", ID, n);
    }

    isRunning_ = false;
}
