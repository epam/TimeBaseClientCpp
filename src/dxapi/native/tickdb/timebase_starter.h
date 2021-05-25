/*
  Copyright 2021 EPAM Systems, Inc

  See the NOTICE file distributed with this work for additional information
  regarding copyright ownership. Licensed under the Apache License,
  Version 2.0 (the "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
  License for the specific language governing permissions and limitations under
  the License.
 */
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