// Regression test for ImOperator::DistByCorrelation's FFT-shift
// (src/im/imoperator.cpp), the cv::dft-ported replacement for IM's
// imProcessCrossCorrelation used by superimposition registration.
//
// For an odd cols/rows, the quadrant swap that recenters the
// zero-shift correlation peak used mismatched block sizes: it swapped
// [0, cols/2) with [cols/2, cols) as-is, which only lines up with the
// crop step's assumed centre (cols/2, floor) when cols is even. For
// odd cols/rows the peak landed one pixel off from where the crop
// looks for it -- a systematic +-1 pixel bias on whichever axis
// (width and/or height) of the correlation window was odd.
// SubRegister's recursive halving of the search window produces odd
// sizes constantly, not as a rare edge case, which is consistent with
// a reported registration misalignment along one axis only (e.g. a
// purely vertical shift, if the odd dimension happened to be height).
//
// Autocorrelation of any signal peaks at zero lag, regardless of sign
// convention -- so feeding the same image as both inputs must report
// a zero shift no matter the window's size parity. That's exactly the
// property the bug broke.

#include <opencv2/core.hpp>
#include <doctest/doctest.h>

#include "im/imoperator.h"

namespace {

// Exposes the otherwise-protected method for testing.
class TestOperator : public ImOperator {
public:
    void Correlate(const cv::Mat &im1, const cv::Mat &im2, imSize window,
                   int *dx, int *dy, int *maxCorr) {
        DistByCorrelation(im1, im2, window, dx, dy, maxCorr);
    }
};

// A textured, non-periodic pattern so the correlation has a single
// unambiguous peak (a flat or periodic image would have several).
cv::Mat MakePattern(int width, int height) {
    cv::Mat img(height, width, CV_8UC1);
    cv::RNG rng(12345);
    rng.fill(img, cv::RNG::UNIFORM, 0, 256);
    return img;
}

} // namespace

TEST_CASE("DistByCorrelation reports zero shift for identical images, odd or even size") {
    const struct { int w, h; const char *label; } sizes[] = {
        {50, 36, "even x even"},
        {51, 36, "odd x even (X axis affected pre-fix)"},
        {50, 37, "even x odd (Y axis affected pre-fix)"},
        {51, 37, "odd x odd (both axes affected pre-fix)"},
    };

    for (const auto &s : sizes) {
        CAPTURE(s.label);
        cv::Mat im = MakePattern(s.w, s.h);

        TestOperator op;
        int foundX = 1234, foundY = 1234, maxCorr = 0;
        op.Correlate(im, im, imSize(5, 5), &foundX, &foundY, &maxCorr);

        CHECK(foundX == 0);
        CHECK(foundY == 0);
    }
}
