// Repro test for the Windows Superimposition crash: "Superimposition ->
// New Book -> select all images" eventually runs every selected scan
// through AxImage::LoadFile (src/app/aximage.cpp), which was ported from
// IM to OpenCV (cv::resize / cv::Rect crop loop that downsizes oversized
// scans). This test calls that method directly, one real scan at a time,
// on Linux -- if it crashes here too, the bug is portable logic, not a
// Windows-only DLL/toolchain issue, and there's no need to reach for Wine
// or a Windows VM to keep debugging it.
//
// Fixtures are NOT committed (real scans, ~100MB total): unzip
// "Notenvergleich 1 - png.zip" (repo root) into tests/fixtures/sup_pages/
// to populate them. Without fixtures this test is a no-op.

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "aximage.h"

#ifndef AX_SUP_FIXTURES_DIR
#define AX_SUP_FIXTURES_DIR "tests/fixtures/sup_pages"
#endif

TEST_CASE("AxImage::LoadFile survives real oversized scans without crashing") {
    namespace fs = std::filesystem;
    const fs::path dir = AX_SUP_FIXTURES_DIR;

    std::vector<fs::path> images;
    if (fs::exists(dir)) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".png")
                images.push_back(entry.path());
        }
    }
    std::sort(images.begin(), images.end());

    if (images.empty()) {
        MESSAGE("no fixtures in " << dir.string()
                << " -- unzip 'Notenvergleich 1 - png.zip' into it to run "
                   "this test; skipping.");
        return;
    }

    // Matches AxApp::OnInit's defaults (src/app/axframe.cpp), so the
    // reduction loop in LoadFile actually triggers for these scans.
    AxImage::s_reduceBigImages = true;
    AxImage::s_imageSizeToReduce = 3000;

    for (const auto& path : images) {
        const std::string name = path.filename().string();
        CAPTURE(name);

        // Flushed *before* LoadFile so a hard crash still tells us which
        // file it happened on.
        std::fprintf(stdout, "loading %s ...\n", name.c_str());
        std::fflush(stdout);

        AxImage img;
        bool ok = img.LoadFile(wxString::FromUTF8(path.string()));

        std::fprintf(stdout, "  -> ok=%d size=%dx%d\n", ok,
                     img.GetWidth(), img.GetHeight());
        std::fflush(stdout);

        if (!ok) continue;  // decode failure is a separate, non-crash bug

        CHECK(img.IsOk());
        CHECK(img.GetWidth() > 0);
        CHECK(img.GetHeight() > 0);
        CHECK(std::max(img.GetWidth(), img.GetHeight())
              <= AxImage::s_imageSizeToReduce);
    }
}
