// Custom main for the AxImage::LoadFile integration test binary.
//
// This target links wxWidgets (unlike aruspix_tests, which is wx-free),
// so it needs wx's image handlers registered before any wxImage::LoadFile
// call can decode a PNG/JPEG. wxInitializer sets up just enough of wx to
// do that without a full wxApp/event loop.

#define DOCTEST_CONFIG_IMPLEMENT
#include <cstdio>

#include <doctest/doctest.h>
#include <wx/init.h>
#include <wx/image.h>

int main(int argc, char** argv) {
    wxInitializer initializer;
    if (!initializer.IsOk()) {
        std::fprintf(stderr, "Failed to initialize wxWidgets\n");
        return 1;
    }
    wxInitAllImageHandlers();

    return doctest::Context(argc, argv).run();
}
