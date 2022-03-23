/*
 * Copyright 2021 EPAM Systems, Inc
 *
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */
#pragma once

#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include "util/srw_lock.h"
#include "util/concurrent_object_set.h"
#include <unordered_map>
#include <thread>

namespace DxApiImpl {
    class TickLoaderImpl;
    class TickDbImpl;

    typedef void (*LoaderListenerCallback)(DxApi::TickLoader * loader);

    class LoaderManager {

    public:
        // Returns true if this instance of the manager is running
        bool isRunning() const;

        // Add loader to the manager. The manager will poll loader's input stream (socket).
        void add(TickLoaderImpl * loader);

        // Remove loader from the list, delete all its listeners
        // Will wait until the loader's input steram is closed if the waitForCompletion flag is true
        bool remove(TickLoaderImpl * loader, bool waitForCompletion);

        // Add  listener for running loader
        void addListener(TickLoaderImpl * loader, LoaderListenerCallback callback);

        // remove listener for a loader
        void removeListener(TickLoaderImpl * loader, LoaderListenerCallback callback);

       /* bool start();
       */

        void stop();

        bool start(); // Supposed to be only called once when the initialization of the parent object is complete

        size_t loadersCount();

        LoaderManager(TickDbImpl &db);
        ~LoaderManager();
        
    protected:
        static void threadProcStatic(LoaderManager &);
        void process(TickLoaderImpl *);
        void processAll();
        void threadProc();

        TickLoaderImpl * find(TickLoaderImpl *);
        void removeLoaderInternal(TickLoaderImpl * x);

    protected:
        TickDbImpl &db_;
        dx_thread::srw_lock thisLock_;
        std::thread * thread_;
        volatile bool isRunning_, shouldStop_;

        concurrent_ptr_set loaders_, being_removed_;

    private:
        int loaderCloseTimeout_;

    };
};