// Integration regression test for ImRegister::Register (src/im/imregister.cpp),
// the cv::dft-ported cross-correlation registration step that "Batch
// superimposition" runs per image pair.
//
// Covers a real crash: SubRegister used to crop/paste at a
// correlation-shifted offset with a raw cv::Rect. At a border recursion
// cell the crop origin is pinned to 0 (to reach the image edge), so any
// shift back towards that edge went negative -- cv::Mat's Rect bounds
// are strict, so this threw an unhandled cv::Exception. It reproduced on
// every real image pair tried, not just as a rare edge case. Fixed by
// ax::set_data()/ax::get_data() (clamped paste/read) instead of raw
// Rects; see also the unit-level coverage in test_image_ops.cpp.
//
// Fixtures are real scans, NOT committed (~100MB+): unzip
// "Notenvergleich 1 - png.zip" and "Notenvergleich 2 - png.zip" (repo
// root, also untracked) into tests/fixtures/sup_pages/ to populate them.
// Without fixtures every case here is a no-op.

#include <cstdio>
#include <filesystem>
#include <string>

#include <doctest/doctest.h>

#include "app/axprogressdlg.h"
#include "im/imregister.h"

#ifndef AX_SUP_FIXTURES_DIR
#define AX_SUP_FIXTURES_DIR "tests/fixtures/sup_pages"
#endif

namespace {

// Runs Init -> DetectPoints -> Register on one pair, from files named
// relative to AX_SUP_FIXTURES_DIR. Returns false (skips, not fails) if
// either fixture file is missing -- these are local-only real scans.
bool RunPair(const char* file1, const char* file2) {
    namespace fs = std::filesystem;
    const fs::path dir = AX_SUP_FIXTURES_DIR;
    const fs::path path1 = dir / file1;
    const fs::path path2 = dir / file2;

    if (!fs::exists(path1) || !fs::exists(path2)) {
        MESSAGE("skipping (fixture missing): " << path1.string() << " / "
                << path2.string());
        return false;
    }

    bool modified = false;
    ImRegister reg(wxT("/tmp/aruspix-imregister-test-scratch/"), &modified);
    AxProgressDlg dlg;
    reg.SetProgressDlg(&dlg);

    REQUIRE_MESSAGE(
        reg.Init(wxString::FromUTF8(path1.string()), wxString::FromUTF8(path2.string())),
        "Init failed (error=" << reg.GetError() << ") for " << file1 << " / " << file2);

    imPoint points1[4], points2[4];
    if (!reg.DetectPoints(points1, points2)) {
        // Legitimate data-dependent failure (e.g. mismatched staff
        // counts between the two scans), not the crash under test.
        MESSAGE("DetectPoints failed (error=" << reg.GetError() << ") for "
                << file1 << " / " << file2 << " -- skipping Register");
        return true;
    }

    bool register_ok = false;
    try {
        register_ok = reg.Register(points1, points2);
    } catch (const cv::Exception& e) {
        FAIL_CHECK("cv::Exception escaped Register() for " << file1 << " / "
                   << file2 << ": " << e.what());
        return true;
    }

    if (register_ok) {
        CHECK(!reg.m_result.empty());
    } else {
        // A clean, handled failure (Terminate) is acceptable -- only an
        // escaping exception is the bug this test guards against.
        MESSAGE("Register reported failure (error=" << reg.GetError() << ") for "
                << file1 << " / " << file2);
    }
    return true;
}

}  // namespace

TEST_CASE("ImRegister::Register: near-duplicate scans of the same folio") {
    RunPair("07. 11a_D-Rs_Folio6r copy.png", "08. 11a_D-Rs_Folio6r copy.png");
}

TEST_CASE("ImRegister::Register: same folio position across libraries (Notenvergleich 1)") {
    RunPair("11. 11b_D-Sl_fol6r copy.png", "14. 11c_D-Mbs_Folio6r copy.png");
}

TEST_CASE("ImRegister::Register: cross-zip pair reported crashing (Notenvergleich 1 vs 2)") {
    RunPair("07. 11a_D-Rs_Folio6r copy.png", "zip2_07. 11b_D-Sl_fol6r copy.png");
}
