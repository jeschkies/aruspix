// Tests for the cv::Mat-native free functions in src/im/image_ops.h.

#include <doctest/doctest.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "image_ops.h"

// ---------------------------------------------------------------------------
// ax::remove_by_area — zero connected components whose area is out of range
// ---------------------------------------------------------------------------

TEST_CASE("remove_by_area: small components below min_area are cleared") {
    // 10x10 image: one 3x3 block (area 9) and one 1x1 pixel (area 1).
    cv::Mat img = cv::Mat::zeros(10, 10, CV_8UC1);
    img(cv::Rect(0, 0, 3, 3)).setTo(1);
    img.at<uchar>(6, 6) = 1;

    cv::Mat out;
    ax::remove_by_area(img, out, /*connectivity=*/8,
                       /*min_area=*/5, /*max_area=*/0);

    // The 3x3 block survives.
    CHECK(out.at<uchar>(0, 0) == 1);
    CHECK(out.at<uchar>(2, 2) == 1);
    // The 1x1 pixel is gone.
    CHECK(out.at<uchar>(6, 6) == 0);
}

TEST_CASE("remove_by_area: components above max_area are also cleared") {
    // Small block (area 4) survives; large block (area 25) is cleared.
    cv::Mat img = cv::Mat::zeros(10, 10, CV_8UC1);
    img(cv::Rect(0, 0, 2, 2)).setTo(1);
    img(cv::Rect(4, 4, 5, 5)).setTo(1);

    cv::Mat out;
    ax::remove_by_area(img, out, /*connectivity=*/8,
                       /*min_area=*/2, /*max_area=*/10);

    CHECK(out.at<uchar>(0, 0) == 1);  // small survives
    CHECK(out.at<uchar>(4, 4) == 0);  // large cleared
    CHECK(out.at<uchar>(8, 8) == 0);
}

TEST_CASE("remove_by_area: max_area=0 disables the upper bound") {
    // A gigantic component should survive when max_area is 0.
    cv::Mat img = cv::Mat::ones(10, 10, CV_8UC1);

    cv::Mat out;
    ax::remove_by_area(img, out, /*connectivity=*/8,
                       /*min_area=*/1, /*max_area=*/0);

    // Whole image is one component, area 100. Should survive.
    CHECK(cv::countNonZero(out) == 100);
}

TEST_CASE("remove_by_area: in-place is a no-op if the mat matches") {
    cv::Mat img = cv::Mat::zeros(6, 6, CV_8UC1);
    img.at<uchar>(2, 2) = 1;  // area 1

    ax::remove_by_area(img, img, /*connectivity=*/4,
                       /*min_area=*/2);

    CHECK(img.at<uchar>(2, 2) == 0);
}

// ---------------------------------------------------------------------------
// ax::set_data / ax::get_data — clamped paste/read, in either direction
//
// Regression coverage for a real crash: ImRegister::SubRegister (the
// cv::dft-based registration's recursive step) used to paste/read at a
// correlation-shifted offset with a raw cv::Rect, which throws when the
// shift pushes the region past the target Mat's edge -- something that
// happens in completely ordinary registrations (e.g. a small shift at a
// border cell where the move origin is pinned to 0). set_data/get_data
// clip instead of throwing, matching the forgiving behaviour the old
// IM-based crop almost certainly had.
// ---------------------------------------------------------------------------

TEST_CASE("set_data: fully in-bounds paste copies everything") {
    cv::Mat img = cv::Mat::zeros(10, 10, CV_8UC1);
    cv::Mat sel = cv::Mat::ones(3, 3, CV_8UC1) * 7;

    ax::set_data(img, sel, 2, 2);

    CHECK(cv::countNonZero(img) == 9);
    CHECK(img.at<uchar>(2, 2) == 7);
    CHECK(img.at<uchar>(4, 4) == 7);
    CHECK(img.at<uchar>(1, 1) == 0);
}

TEST_CASE("set_data: negative offset clips instead of throwing") {
    cv::Mat img = cv::Mat::zeros(10, 10, CV_8UC1);
    cv::Mat sel = cv::Mat::ones(5, 5, CV_8UC1) * 9;

    // Paste at (-2, -3): only the bottom-right 3x2 corner of `sel`
    // should land, at the image's (0, 0).
    ax::set_data(img, sel, -2, -3);

    CHECK(img.at<uchar>(0, 0) == 9);
    CHECK(img.at<uchar>(1, 2) == 9);
    CHECK(cv::countNonZero(img) == 3 * 2);
}

TEST_CASE("set_data: oversize write is truncated to the image bounds") {
    cv::Mat img = cv::Mat::zeros(5, 5, CV_8UC1);
    cv::Mat sel = cv::Mat::ones(10, 10, CV_8UC1) * 3;

    ax::set_data(img, sel, 2, 2);

    CHECK(img.at<uchar>(2, 2) == 3);
    CHECK(img.at<uchar>(4, 4) == 3);
    CHECK(cv::countNonZero(img) == 3 * 3);  // only rows/cols 2..4 fit
}

TEST_CASE("set_data: fully out-of-range offset is a no-op") {
    cv::Mat img = cv::Mat::zeros(5, 5, CV_8UC1);
    cv::Mat sel = cv::Mat::ones(3, 3, CV_8UC1);

    ax::set_data(img, sel, 100, 100);

    CHECK(cv::countNonZero(img) == 0);
}

TEST_CASE("get_data: fully in-bounds read copies everything") {
    cv::Mat img(10, 10, CV_8UC1, cv::Scalar(5));
    cv::Mat sel = cv::Mat::zeros(3, 3, CV_8UC1);

    ax::get_data(img, sel, 2, 2);

    CHECK(cv::countNonZero(sel) == 9);
    CHECK(sel.at<uchar>(0, 0) == 5);
}

TEST_CASE("get_data: negative offset reads only the in-bounds part") {
    cv::Mat img(10, 10, CV_8UC1, cv::Scalar(9));
    cv::Mat sel = cv::Mat::zeros(5, 5, CV_8UC1);  // pre-zeroed sentinel

    // Read from (-2, -3): matches the border-shift case in SubRegister --
    // only the part of `sel` whose source falls inside `img` gets
    // written; the rest keeps its pre-existing (zeroed) content. The
    // clipped-out offset (2, 3) lands the copied region at
    // sel rows [3,5) x cols [2,5) -- Mat::at is (row, col) i.e. (y, x).
    ax::get_data(img, sel, -2, -3);

    CHECK(sel.at<uchar>(3, 2) == 9);   // in-bounds: copied
    CHECK(sel.at<uchar>(4, 4) == 9);   // in-bounds: copied
    CHECK(sel.at<uchar>(0, 0) == 0);   // out-of-bounds: left as sentinel
    CHECK(sel.at<uchar>(2, 4) == 0);   // out-of-bounds: left as sentinel
    CHECK(cv::countNonZero(sel) == 3 * 2);
}

TEST_CASE("get_data: fully out-of-range offset is a no-op") {
    cv::Mat img(5, 5, CV_8UC1, cv::Scalar(9));
    cv::Mat sel = cv::Mat::zeros(3, 3, CV_8UC1);

    ax::get_data(img, sel, 100, 100);

    CHECK(cv::countNonZero(sel) == 0);
}

TEST_CASE("remove_by_area: 4-connectivity treats diagonals as separate") {
    // Two 1-pixel components diagonally adjacent. With 4-connectivity
    // they are separate components (each area 1). With 8-connectivity
    // they would be one (area 2).
    cv::Mat img = cv::Mat::zeros(5, 5, CV_8UC1);
    img.at<uchar>(1, 1) = 1;
    img.at<uchar>(2, 2) = 1;

    cv::Mat out;
    ax::remove_by_area(img, out, /*connectivity=*/4, /*min_area=*/2);
    // Both should be cleared (each has area 1 alone).
    CHECK(cv::countNonZero(out) == 0);

    ax::remove_by_area(img, out, /*connectivity=*/8, /*min_area=*/2);
    // Both survive as a single 8-connected component of area 2.
    CHECK(cv::countNonZero(out) == 2);
}
