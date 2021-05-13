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
