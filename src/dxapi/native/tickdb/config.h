#pragma once

// If 1, program will log native exceptions and some other events to log textfile
// If such file found in the same directory as the executable itself, debug logging is enabled automatically, regardless of the value of this flag
#define DBG_LOG_ENABLED 1

#define LOG_START_MSG "DEBUG VER. Logger initialized for:"

// Filename suffix for main log file
#define LOG_FILE_NAME_PREFIX "dxapilog_"

#if defined(DEBUG) || defined(_DEBUG)
#define LOG_FILE_NAME_SUFFIX "dbg"
#else
#define LOG_FILE_NAME_SUFFIX "rel"
#endif

#define LOG_FILE_EXT "txt"

//#define LOG_FORCE_LOGGING

// More logging in TickLoader. Valid values : 0, 1, 2
// Level 1: Log significant state changes, subscription executions, creation, deletion
// Level 2: Will log all data sent to separate files for messages and written blocks
#define VERBOSE_LOADER 1

// More logging in TickCursor. Valid values : 0, 1, 2
// Level 1: Log significant state changes, subscription executions, creation, deletion
// Level 2: Will log received messages info (before subscription filter) to log file
// Level 3: Will log received messages (before subscription filter) to separate files for messages and HTTP chunks
// NOTE: In the current implementation messages skipped due to reset request may still not be logged
#define VERBOSE_CURSOR 1

// More logging of subscription-related data (to main log file)
// Level 1: Will log every change of subscriptions 
// Level 2: Will log if every received message is filtered or not
// In the current implementation messages skipped due to reset request may still not be logged
// If you want to log the contents of filtered messages you should also set VERBOSE_CURSOR to 2
#define VERBOSE_CURSOR_FILTERS 1


// More logging in Sockets. Valid values : 0, 1
#define VERBOSE_SOCKETS 1

#define VERBOSE_TICKDB 1

// Verbose TickDb. Valid values : 0, 1, 2
#define VERBOSE_TICKDB_MANAGER_THREAD 1


// More logging in XML-related code. Valid values : 0, 1, 2
// Level 2:  Will log all sent xml requests and selects.
// Level 1:  Will only log requests that caused errors.
#define XML_LOG_LEVEL 1

/***************************************************************************************************************
 * Timeouts
 */


// Loader will wait this much until nobody writes messages for at least X ms
#define LOADER_STOPPING_WAIT_MIN_SLEEP_MS 5
// Loader will wait until active client stops for this amount of ms
#define LOADER_STOPPING_WAIT_MAX_SLEEP_MS 50
// Loader will wait until active client stops for this amount of ms
#define LOADER_STOPPING_WAIT_TIMEOUT_MS 12000


// Cursor will wait this much until nobody writes messages for at least X ms
#define CURSOR_STOPPING_WAIT_MIN_MS 2
// Cursor will wait until active client stops for this amount of ms
#define CURSOR_STOPPING_WAIT_MAX_MS 8000



/***************************************************************************************************************
 * Debuging options. Don't enable unless you know what you are doing.
 */

// Makes TickLoader ignore data instead of sending it. Useful for benchmarks. The data is still written to the buffer.
#define LOADER_DONT_SEND 0

#define DBG_DELAY_LIVE_CURSOR_DISCONNECTION_EXCEPTION_MS 0

// Do not send subscription change requests to the server, only use client-side filters to remove instruments/types/streams from subscription set
// (won't be able to add entities & streams that weren't present in the original select request)
//#define DBG_SKIP_SUBSCRIPTION_CHANGE_REQUESTS 1

#define DBGLOG_ISSUE3114 (void)

// Uncomment this to enable logging for this issue
//#define DBGLOG_ISSUE3114 DBGLOG

// Call Debugger::Break in C++/CLI wrapper for .select() on this stream
//#define DBG_CATCH_STREAM "#events"

/***************************************************************************************************************
 * Current transitional options. Should be changed to desired state after adding/debugging some internal features
 */

#define MANAGEMENT_THREAD_ENABLED 0

#define DBG_HEAP_ENABLED 0

#define DELAYED_IO_EXCEPTIONS 0

#define RETHROW_LOADER_EVENT_EXCEPTIONS 1

// If enabled, NULL fields at the end of a message will be trimmed. Will be enabled after additional testing.
// TODO: Test
#define LOADER_TRIM_NULL_FIELDS 0

// Periodically flush backtesting loaders
#define LOADER_FLUSH_TIMER_ENABLED 0

// Always flush after finishing a message
#define TICKLOADER_FORCE_FLUSH 0

// Always flush and set TCP_NO_DELAY=true to guarantee realtime delivery, overriding LoadingOptions
#define TICKLOADER_FORCE_MINLATENCY 0

#define USE_SH 1


/***************************************************************************************************************
 * Deprecated transitional options. Do not change as they'll cause regressions.
 */

#define LOADER_OBJ_IMPL 2

// Disable setting 'realtime notification' to true for SelectionOptions to guarantee that this message type never appears
#define REALTIME_NOTIFICATION_DISABLED 0

// If 1, TickLoader will send nanotime in headers (deprecated)
#define LOADER_NANOSECOND_TIME 1


// Disable most logging options in release anyway
#if (!defined(_DEBUG) || DBG_LOG_ENABLED != 1) && !defined(LOG_FORCE_LOGGING)
#undef VERBOSE_LOADER
#undef VERBOSE_SOCKETS
#undef VERBOSE_CURSOR
#undef VERBOSE_TICKDB
#undef VERBOSE_TICKDB_MANAGER_THREAD
#undef DBG_LOG_FILTERED_MESSAGES
#undef DBG_SKIP_SUBSCRIPTION_CHANGE_REQUESTS
#define VERBOSE_LOADER 0
#define VERBOSE_SOCKETS 0
#define VERBOSE_CURSOR 0
#define VERBOSE_TICKDB 0
#define VERBOSE_TICKDB_MANAGER_THREAD 0
#endif