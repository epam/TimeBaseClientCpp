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
// Contains minumum amount of public includes from windows.h that HAS to have global visibility

typedef unsigned long       DWORD;
#if defined(_WIN64)
    typedef unsigned __int64 SOCKET;
#else
    typedef unsigned int SOCKET;

#endif

#define INVALID_SOCKET  (SOCKET)(~0)

extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();