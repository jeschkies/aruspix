// Custom main for the ImRegister integration test binary.
//
// Unlike aruspix_tests / aximage_loadfile_tests, this target drives the
// real, wx-coupled ImRegister/ImPage pipeline (built with -DAX_CMDLINE
// so AxProgressDlg is the non-GUI stub -- see src/app/axprogressdlg.h).
// It still needs wx's image handlers registered to decode the PNG
// fixtures, via wxInitializer (no full wxApp/event loop needed).

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
