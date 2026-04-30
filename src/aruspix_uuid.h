#pragma once

#if defined(_WIN32)

#include <cstdio>
#include <cstring>
#include <rpc.h>

typedef unsigned char uuid_t[16];

inline void uuid_generate(uuid_t out)
{
    UUID u;
    UuidCreate(&u);
    std::memcpy(out, &u, 16);
}

inline void uuid_clear(uuid_t out)
{
    std::memset(out, 0, 16);
}

inline int uuid_compare(const uuid_t a, const uuid_t b)
{
    return std::memcmp(a, b, 16);
}

inline void uuid_copy(uuid_t dst, const uuid_t src)
{
    std::memcpy(dst, src, 16);
}

inline int uuid_is_null(const uuid_t in)
{
    for (int i = 0; i < 16; ++i)
        if (in[i] != 0) return 0;
    return 1;
}

inline void uuid_unparse(const uuid_t in, char *out)
{
    std::snprintf(out, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        in[0], in[1], in[2], in[3],
        in[4], in[5],
        in[6], in[7],
        in[8], in[9],
        in[10], in[11], in[12], in[13], in[14], in[15]);
}

inline int uuid_parse(const char *in, uuid_t out)
{
    unsigned int b[16];
    if (std::sscanf(in,
            "%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x",
            &b[0], &b[1], &b[2], &b[3],
            &b[4], &b[5], &b[6], &b[7],
            &b[8], &b[9], &b[10], &b[11],
            &b[12], &b[13], &b[14], &b[15]) != 16)
        return -1;
    for (int i = 0; i < 16; ++i) out[i] = static_cast<unsigned char>(b[i]);
    return 0;
}

#else

#include <uuid/uuid.h>

#endif
