#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include <conio.h>
#include <memory>
#include <thread>
#include <chrono>

#include "connection.h"

using namespace std;
using namespace DxApi;

typedef Connection C;

// TODO: Move code here



void cursorStopThread(TickCursor ** cursor, volatile bool * volatile stop, bool shouldYield, bool shouldDelete);
void loaderStopThread(TickLoader ** cursor, volatile bool * volatile stop, bool shouldYield, bool shouldDelete);


template <class T>
void concurrentStopThread(T ** entity, volatile bool * volatile stop, bool shouldDelete, bool shouldYield)
{
    while (!*stop) {
        if (shouldYield) {
            this_thread::yield();
        }
    }

    try {
        if (shouldDelete) {
            delete *entity;
        }
        else {
            (*entity)->close();
        }
    }
    catch (exception &e) {
        printf("Cursor thread: %s\n", e.what());
    }
    catch (...) {
        puts("Cursor thread:System exception");
    }
}


template <class T> void concurrentStopAll(bool shouldDelete, bool shouldYield, vector<T *> &objects)
{
    vector<thread> threads;
    volatile bool stop = false;
    forn(i, objects.size()) {
        threads.emplace_back(concurrentStopThread<T>, &objects[i], &stop, shouldDelete, shouldYield);
    }

    this_thread::sleep_for(chrono::milliseconds(1000));
    stop = true;

    forn(i, threads.size()) {
        threads[i].join();
    }
}


void C::concurrentStopAllCursors(bool shouldDelete, bool shouldYield)
{
    concurrentStopAll(shouldDelete, shouldYield, cursors);
}


void C::concurrentStopAllLoaders(bool shouldDelete, bool shouldYield)
{
    concurrentStopAll(shouldDelete, shouldYield, loaders);
}