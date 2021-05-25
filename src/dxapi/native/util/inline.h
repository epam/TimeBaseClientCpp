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

#if defined(_MSC_VER)
// Visual C
#ifndef INLINE
#define NOINLINE __declspec(noinline)
#if (_MSC_VER >= 1200)
// For newer Visual C
#define INLINE __forceinline
#else
// For older Visual C
#define INLINE __inline
#endif /* #if (_MSC_VER >= 1200) */
#endif /* #ifndef INLINE */

#else
// GCC/Clang
#ifndef INLINE
#define INLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__ ((noinline))
#endif
#endif