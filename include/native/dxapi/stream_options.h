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

#include "dxcommon.h"
#include <stdint.h>
#include <sstream>
#include <string>

#include "buffer_options.h"

namespace DxApi {

    struct StreamScopeEnum {
        enum Enum {
            ///////
            /// All messages are stored in TimeBase
            ////
            DURABLE,

            ///////
            /// All messages are stored in external data file.
            ////
            EXTERNAL_FILE,

            ///////
            /// The stream does not store data on disk, but its key and
            /// structure are durable.
            ////
            TRANSIENT,

            ///////
            /// The stream does not leave any permanent trace
            ////
            RUNTIME
        };
    };

    ENUM_CLASS(uint8_t, StreamScope);


    class StreamOptions {
    public:
        /**
         *  "Maximum" distribution factor constant.
         */
        enum {
            
            MAX_DISTRIBUTION = 0
        };

        /**
         *  Optional user-readable name.
         */
        Nullable<std::string> name;

        /**
         *  Optional multi-line description.
         */
        Nullable<std::string> description;

        /**
         *  Optional owner of stream.
         *  During stream creation it will be set
         *  equals to authenticated user name.
         */
        Nullable<std::string> owner;

        /**
         *  Location of the stream (by default empty). When defined this attribute provides alternative stream location (rather than default location under QuantServerHome)
         */
        Nullable<std::string> location;

        /**
         * Options that control data buffering.
         */
        BufferOptions bufferOptions;

        StreamScope  scope;

        /**
         *
         *  The number of M-files into which to distribute the
         *  data. Supply {@link #MAX_DISTRIBUTION} to keep a separate file
         *  for each instrument (default).
         */
        int32_t distributionFactor;

        Nullable<std::string> distributionRuleName; // NOT SERIALIZED

        /**
        * Indicates that loader will ignore binary similar messages(for 'unique' streams only).
        */
        bool duplicatesAllowed;

        /**
         *  High availability durable streams are cached on startup.
         */
        bool highAvailability; // NOT SERIALIZED

        /**
        * Determines that stream will contains unique messages.
        */
        bool unique;
        bool polymorphic;

        /**
         *  Stream periodicity, if known.
         */
        std::string periodicity; 
        //public Periodicity                  periodicity = Periodicity.mkIrregular();


        // Contains XML metadata
        Nullable<std::string> metadata;

        bool operator==(const StreamOptions &value) const;

        StreamOptions() :
            name(),
            description(),
            owner(),
            location(),
            
            scope(StreamScope::DURABLE),
            distributionFactor(0),
            distributionRuleName(),
            duplicatesAllowed(true),
            highAvailability(false),
            unique(false),
            polymorphic(false),
            periodicity("IRREGULAR"),
            metadata()
        { }
    };
}