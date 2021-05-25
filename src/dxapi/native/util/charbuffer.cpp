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
#include "charbuffer.h"
#include <string.h>

static void copy_clipped(char *dest, const char *src, size_t dest_size,  size_t src_size)
{
    src_size = src_size > dest_size ? dest_size : src_size;
    memcpy(dest, src, src_size);
    dest[src_size] = '\0';
}


static void copy_shortened(char *dest, const char *src, size_t dest_size, size_t src_size)
{
    copy_clipped(dest, src, dest_size, src_size);
    if (src_size > dest_size && dest_size > 2) {
        dest[dest_size - 2] = dest[dest_size - 1] = '.';
    }
    
}


void CharBufferImpl::copy_shortened(char dest[], const std::string &s, size_t n)
{
    ::copy_shortened(dest, s.data(), n, s.length());
}


void CharBufferImpl::copy_clipped(char dest[], const std::string &s, size_t n)
{
    ::copy_clipped(dest, s.data(), n, s.length());
}


void CharBufferImpl::copy_shortened(char dest[], const char *s, size_t n)
{
    ::copy_shortened(dest, s, n, strlen(s));
}


void CharBufferImpl::copy_clipped(char dest[], const char *s, size_t n)
{
    ::copy_clipped(dest, s, n, strlen(s));
}
