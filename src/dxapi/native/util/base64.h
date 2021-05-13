#include <stdint.h>

#ifdef __cplusplus
#include <string>
namespace Base64MIME {
    extern "C" {
#endif  /* __cplusplus */

#ifndef INLINE
#define INLINE
#endif

        static void base64_encode(char * dest, const char * src, size_t n_src)
        {
            static const char base64chars[0x41] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            while (0 != n_src) {
                uintptr_t m = (n_src > 3 ? 3 : n_src), i = m, x = 0;
                n_src -= m;
                for (; i != 0; --i) {
                    x = (x << 8) | *(const uint8_t *)(src++);
                }

                x <<= 2 * (3 - m);
                dest[2] = dest[3] = '=';
                do {
                    dest[m] = base64chars[x & 0x3F];
                    x >>= 6;
                } while (m-- != 0);

                dest += 4;
            }
        }

#ifdef __cplusplus
    }

    static void base64_encode(std::string &dest, const char * src, size_t n_src)
    {
        if (0 == n_src) {
            return dest.clear();
        }

        dest.resize((n_src + 2) / 3U * 4);
        return base64_encode(&dest[0], src, n_src);
    }

    INLINE void base64_encode(std::string &dest, const std::string &src)
    {
        return base64_encode(dest, src.data(), src.size());
    }

    // Less secure, because copy may occur
    INLINE std::string base64_encode(const std::string &src)
    {
        std::string s;
        base64_encode(s, src.data(), src.size());
        return s;
    }
}
#endif  /* __cplusplus */