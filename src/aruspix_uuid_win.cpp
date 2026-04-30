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

// Function name and signature must match the declaration in
// aruspix_uuid.h (`void uuid_generate(ax_uuid_t out)` where ax_uuid_t
// is unsigned char[16]). The array decays to `unsigned char *`, which
// is what we accept here. We deliberately do not include
// aruspix_uuid.h to avoid pulling in the typedef alongside <rpc.h>'s
// own uuid_t.
extern void uuid_generate(unsigned char *out);

void uuid_generate(unsigned char *out)
{
    UUID u;
    ::UuidCreate(&u);
    std::memcpy(out, &u, 16);
}

#endif
