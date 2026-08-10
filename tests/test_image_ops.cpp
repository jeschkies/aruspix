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
