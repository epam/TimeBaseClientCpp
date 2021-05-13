#define _CRT_SECURE_NO_WARNINGS

#include <memory>
#include <string>
#include <cctype>
#include <cstdio>
#include "tickdb/common.h"
//#define WIN32_LEAN_AND_MEAN
//#include <Windows.h>

using namespace std;


#define _CDECL __cdecl

FILE * f, *g;


// Copy length bytes from stdin to stdout, return true if all bytes are copied, false if EOF etc.
bool copy(size_t length)
{
    int c;
    while (length-- && ((c = getchar()) >= 0)) {
        putchar(c);
    }
    return 0 == (length + 1);
}


int hexdigit(int c)
{
    unsigned u = c;
    return u - '0' < 10 ? u - '0' : u - 'a' < 6 ? u + (10 - 'a') : u - 'A' < 6 ? u + (10 - 'A') : -1;
}


bool is_hexdigit(int c)
{
    return hexdigit(c) >= 0;
}

int _CDECL main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "unchunk: remove HTTP chunk headers from a binary file\n", argv[1]);
        fprintf(stderr, "Usage unchunk <infile> <outfile>\n", argv[1]);
    }

    f = freopen(argv[1], "rb", stdin);
    if (NULL == f) {
        fprintf(stderr, "Enable to open input file %s\n", argv[1]);
        return errno;
    }

    g = freopen(argv[2], "wb", stdout);

    if (NULL == g) {
        fprintf(stderr, "Enable to create output file %s\n", argv[2]);
        return errno;
    }

    vector<char> chars;
    size_t length;
    chars.clear();
    int c;

    c = getchar();
    if (is_hexdigit(c)) goto parse;
    if ('\xD' != c) goto wrong_char;
    c = getchar();
    if (is_hexdigit(c)) goto parse;
    if ('\xA' != c) goto wrong_char;

parse:
    while (1) {
        length = 0;
        while (is_hexdigit(c)) {
            length = (length << 4) + hexdigit(c);
            c = getchar();
        }

        if ('\xD' != c) goto wrong_char;
        c = getchar();
        if ('\xA' != c) goto wrong_char;

        if (!copy(length)) {
            fprintf(stderr, "Unable to copy block, input file prematurely ended\n");
            return errno;
        }

        c = getchar();
        if ('\xD' != c) goto wrong_char;
        c = getchar();
        if ('\xA' != c) goto wrong_char;
        c = getchar();
        if (0 == length) {
            return 0;
        }
    }

wrong_char:
    if (c < 0) {
        fprintf(stderr, "Input file prematurely ended while trying to read chunk header\n");
    }
    else {
        fprintf(stderr, "whong character encountered in chunk header: %x\n", c);
    }

    return 1;
}