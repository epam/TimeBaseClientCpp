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
#pragma once

#include "CommonRunTime/Common.h"
#include "CommonRunTime/DateTime.h"

typedef DX_STATUS(*DX_THREAD_ROUTINE)(DX_POINTER Parameter);

#if defined(DX_PLATFORM_WINDOWS)

typedef HANDLE DX_THREAD, *PDX_THREAD;

typedef DWORD DX_THREAD_ID;

#else /* defined(DX_PLATFORM_WINDOWS) */

typedef struct _DX_THREAD
{
    pthread_t ID;
    DX_BOOLEAN IsValid;
} DX_THREAD, *PDX_THREAD;

typedef pthread_t DX_THREAD_ID;

#endif /* !defined(DX_PLATFORM_WINDOWS) */

DX_RT_API
DX_STATUS
DxThreadInit(
DX_THREAD * Thread,
DX_THREAD_ROUTINE Routine,
DX_POINTER Parameter);

DX_RT_API
DX_STATUS
DxThreadJoin(
DX_THREAD * Thread,
DX_TIME_SPAN Timeout);

DX_RT_API
DX_STATUS
DxThreadDispose(
DX_THREAD * Thread);

DX_STATIC
INLINE
DX_BOOLEAN
DxThreadIsValid(
DX_THREAD * Thread)
{
#if defined(DX_PLATFORM_WINDOWS)
    return (Thread != DX_NULL) && (*Thread != (HANDLE)NULL);
#else /* defined(DX_PLATFORM_WINDOWS) */
    return (Thread != DX_NULL) && Thread->IsValid;
#endif /* !defined(DX_PLATFORM_WINDOWS) */
}

DX_STATIC
DX_INLINE
DX_THREAD_ID
DxThreadGetCurrentID()
{
#if defined(DX_PLATFORM_WINDOWS)
    return GetCurrentThreadId();
#else /* defined(DX_PLATFORM_WINDOWS) */
    return pthread_self();
#endif /* !defined(DX_PLATFORM_WINDOWS) */
}

DX_RT_API
DX_STATUS
DxThreadDestroy(
DX_THREAD * Thread);

DX_RT_API
DX_VOID
DxThreadYield();

DX_RT_API
DX_VOID
DxThreadSleep(
DX_TIME_SPAN Time);