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

// On checksum mismatch: load the golden PNG, threshold back to 0/1,
// and count differing pixels. Records a doctest INFO message with the
// mismatch coordinates so the failure output is actionable.
void report_diff(const std::string& golden_path, const cv::Mat& actual) {
    cv::Mat golden_vis = cv::imread(golden_path, cv::IMREAD_GRAYSCALE);
    if (golden_vis.empty()) {
        INFO("golden PNG missing: " << golden_path);
        return;
    }
    if (golden_vis.rows != actual.rows || golden_vis.cols != actual.cols) {
        INFO("size mismatch: golden " << golden_vis.cols << "x"
             << golden_vis.rows << " vs actual " << actual.cols << "x"
             << actual.rows);
        return;
    }
    cv::Mat golden;
    cv::threshold(golden_vis, golden, 127, 1, cv::THRESH_BINARY);
    int diff = 0;
    for (int i = 0; i < golden.total(); ++i)
        if (golden.data[i] != actual.data[i]) ++diff;
    double pct = 100.0 * diff / golden.total();
    INFO("pixel diff: " << diff << " (" << pct << "%) of "
         << golden.total() << " total");
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
            report_diff(golden + "/" + c.image + "_" + c.method + ".png",
                        binary);
        }
        CHECK(actual == it->second);
    }
}
