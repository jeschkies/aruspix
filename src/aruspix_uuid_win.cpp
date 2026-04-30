// Windows-only out-of-line implementation of uuid_generate.
//
// Lives in its own translation unit because <rpc.h> defines a struct
// `uuid_t` that collides with the libuuid-style array typedef in
// aruspix_uuid.h. By not including aruspix_uuid.h here, we avoid the
// conflict; the function signature just takes a raw `unsigned char *`
// (which is what aruspix_uuid.h's `uuid_t` decays to as a parameter).

#if defined(_WIN32)

#include <cstring>
#include <rpc.h>

void uuid_generate(unsigned char *out)
{
    UUID u;
    ::UuidCreate(&u);
    std::memcpy(out, &u, 16);
}

#endif
