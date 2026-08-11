// Integration test: exercise ax::binarize_and_clean on real page images
// and verify byte-identical output against goldens produced by master.
//
// Two-tier comparison:
//   1. Primary: FNV-1a-64 hash of the raw 0/1 pixel buffer, matched
//      against the checksum committed in tests/fixtures/golden/checksums.txt.
//   2. On mismatch, load the golden PNG (0/255) and report which pixels
//      differ so a human can eyeball what changed.
//
// Fixtures live under tests/fixtures/pages/*.jpg (inputs) and
// tests/fixtures/golden/{*.png,checksums.txt} (references from master).

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

#include <doctest/doctest.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "binarize.h"

namespace {

std::uint64_t fnv1a64(const std::uint8_t* data, std::size_t n) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

// Parse checksums.txt: one line per case, "<basename> <method> <hex>".
// Silently skips blanks / '#' comments.
std::unordered_map<std::string, std::uint64_t> load_checksums(
    const std::string& path)
{
    std::unordered_map<std::string, std::uint64_t> out;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string image, method, hex;
        if (!(ss >> image >> method >> hex)) continue;
        std::uint64_t h = 0;
        for (char c : hex) {
            h <<= 4;
            if (c >= '0' && c <= '9') h |= (c - '0');
            else if (c >= 'a' && c <= 'f') h |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') h |= (c - 'A' + 10);
        }
        out[image + "/" + method] = h;
    }
    return out;
}

// Result of a mismatch dump: a human-readable summary plus the paths
// of the two artifacts written. Returned as one string so the calling
// site can attach it to the CHECK via a single INFO scope.
struct MismatchReport {
    std::string summary;
};

// Ensure a directory exists (portable across POSIX and Windows via
// <filesystem>::create_directories). Silently ignores errors so the
// mismatch reporting path stays best-effort.
void ensure_dir(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
}

// Write karsten/opencv's actual output (0->0, 1->255) and a color-coded
// diff overlay against the golden. Returns a one-line summary suitable
// for INFO.
//   both agree it's background : black
//   both agree it's ink        : gray  (128,128,128)
//   golden-only ink            : red   (BGR 0,0,255) — port lost these
//   actual-only ink            : green (BGR 0,255,0) — port gained these
MismatchReport dump_diff(const std::string& fixtures_dir,
                         const std::string& basename,
                         const cv::Mat& actual /*0/1*/,
                         const cv::Mat& golden_bin /*0/1*/) {
    ensure_dir(fixtures_dir + "/actual");
    ensure_dir(fixtures_dir + "/diff");
    const std::string actual_path = fixtures_dir + "/actual/" + basename + ".png";
    const std::string diff_path   = fixtures_dir + "/diff/"   + basename + ".png";

    cv::imwrite(actual_path, actual * 255);

    cv::Mat overlay(actual.rows, actual.cols, CV_8UC3, cv::Scalar(0, 0, 0));
    for (int y = 0; y < actual.rows; ++y) {
        const uchar* a = actual.ptr<uchar>(y);
        const uchar* g = golden_bin.ptr<uchar>(y);
        cv::Vec3b*   o = overlay.ptr<cv::Vec3b>(y);
        for (int x = 0; x < actual.cols; ++x) {
            bool ai = a[x] != 0, gi = g[x] != 0;
            if (ai && gi)        o[x] = cv::Vec3b(128, 128, 128);  // agree ink
            else if (gi && !ai)  o[x] = cv::Vec3b(0, 0, 255);      // red: golden-only
            else if (ai && !gi)  o[x] = cv::Vec3b(0, 255, 0);      // green: actual-only
        }
    }
    cv::imwrite(diff_path, overlay);

    int diff = cv::countNonZero(actual != golden_bin);
    double pct = 100.0 * diff / actual.total();
    std::ostringstream ss;
    ss << diff << " pixels (" << pct << "%) differ; see " << diff_path
       << " (red=golden-only ink, green=actual-only ink) and " << actual_path;
    return {ss.str()};
}

struct Case {
    const char* image;
    const char* method;
    ax::BinarizationMethod enum_method;
};

const Case kCases[] = {
    {"10a_D-Rs_f7v",  "brink",   ax::BinarizationMethod::Brink2Classes},
    {"10a_D-Rs_f7v",  "brink3",  ax::BinarizationMethod::Brink3Classes},
    {"10a_D-Rs_f7v",  "sauvola", ax::BinarizationMethod::Sauvola},
    {"10a_D-Rs_f7v",  "fixed",   ax::BinarizationMethod::FixedAt127},
    {"10b_D-Sl_f7v",  "brink",   ax::BinarizationMethod::Brink2Classes},
    {"10b_D-Sl_f7v",  "brink3",  ax::BinarizationMethod::Brink3Classes},
    {"10b_D-Sl_f7v",  "sauvola", ax::BinarizationMethod::Sauvola},
    {"10b_D-Sl_f7v",  "fixed",   ax::BinarizationMethod::FixedAt127},
    {"10c_A-Win_f7v", "brink",   ax::BinarizationMethod::Brink2Classes},
    {"10c_A-Win_f7v", "brink3",  ax::BinarizationMethod::Brink3Classes},
    {"10c_A-Win_f7v", "sauvola", ax::BinarizationMethod::Sauvola},
    {"10c_A-Win_f7v", "fixed",   ax::BinarizationMethod::FixedAt127},
    {"10d_CH-FF_f7v", "brink",   ax::BinarizationMethod::Brink2Classes},
    {"10d_CH-FF_f7v", "brink3",  ax::BinarizationMethod::Brink3Classes},
    {"10d_CH-FF_f7v", "sauvola", ax::BinarizationMethod::Sauvola},
    {"10d_CH-FF_f7v", "fixed",   ax::BinarizationMethod::FixedAt127},
    {"10e_CZ-Jm_f7v", "brink",   ax::BinarizationMethod::Brink2Classes},
    {"10e_CZ-Jm_f7v", "brink3",  ax::BinarizationMethod::Brink3Classes},
    {"10e_CZ-Jm_f7v", "sauvola", ax::BinarizationMethod::Sauvola},
    {"10e_CZ-Jm_f7v", "fixed",   ax::BinarizationMethod::FixedAt127},
};

// AX_FIXTURES_DIR is set from CMake so the tests find their fixtures
// regardless of the working directory the runner picks.
#ifndef AX_FIXTURES_DIR
#define AX_FIXTURES_DIR "tests/fixtures"
#endif

}  // namespace

TEST_CASE("binarize_and_clean matches master goldens byte-for-byte") {
    const std::string pages = std::string(AX_FIXTURES_DIR) + "/pages";
    const std::string golden = std::string(AX_FIXTURES_DIR) + "/golden";
    auto checksums = load_checksums(golden + "/checksums.txt");

    for (const auto& c : kCases) {
        // Wrap in std::string so CAPTURE prints the text, not the pointer.
        std::string image = c.image;
        std::string method = c.method;
        CAPTURE(image);
        CAPTURE(method);

        cv::Mat gray = cv::imread(pages + "/" + c.image + ".jpg",
                                  cv::IMREAD_GRAYSCALE);
        REQUIRE_FALSE(gray.empty());

        ax::BinarizeAndCleanParams params;
        params.method = c.enum_method;
        cv::Mat binary;
        REQUIRE(ax::binarize_and_clean(gray, binary, params));

        std::uint64_t actual = fnv1a64(binary.data, binary.total());
        auto key = std::string(c.image) + "/" + c.method;
        auto it = checksums.find(key);
        REQUIRE_MESSAGE(it != checksums.end(),
                        "no golden checksum for " << key);

        if (actual != it->second) {
            const std::string basename = std::string(c.image) + "_" + c.method;
            cv::Mat golden_vis = cv::imread(golden + "/" + basename + ".png",
                                            cv::IMREAD_GRAYSCALE);
            std::string report;
            if (golden_vis.empty()) {
                report = "golden PNG missing at " + golden + "/" + basename + ".png";
            } else if (golden_vis.rows != binary.rows ||
                       golden_vis.cols != binary.cols) {
                std::ostringstream ss;
                ss << "size mismatch: golden " << golden_vis.cols << "x"
                   << golden_vis.rows << " vs actual " << binary.cols << "x"
                   << binary.rows;
                report = ss.str();
            } else {
                cv::Mat golden_bin;
                cv::threshold(golden_vis, golden_bin, 127, 1, cv::THRESH_BINARY);
                report = dump_diff(AX_FIXTURES_DIR, basename, binary,
                                   golden_bin).summary;
            }
            INFO("mismatch: " << report);
        }
        CHECK(actual == it->second);
    }
}
