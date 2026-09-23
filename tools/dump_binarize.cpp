// Standalone tool: load a JPG, run ax::binarize_and_clean with the
// requested method, write the binary result as a PNG (0 -> 0, 1 -> 255)
// and print the FNV-1a-64 hash of the raw 0/1 pixel buffer to stdout.
//
// Used to generate golden fixtures for the integration test suite. A
// twin lives on master, which runs the IM-based algorithm and produces
// the same PNG + hash format so we can compare byte-for-byte.
//
// Usage:
//   dump_binarize <input.jpg> <method> <output_dir>
//     method: brink | brink3 | sauvola | fixed

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "im/binarize.h"

namespace {

// FNV-1a 64-bit hash. Trivially portable, deterministic across
// compilers — good enough for change detection between master (IM)
// and karsten/opencv (OpenCV) outputs.
std::uint64_t fnv1a64(const std::uint8_t* data, std::size_t n) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

bool parse_method(const std::string& s, ax::BinarizationMethod& out) {
    if (s == "brink")   { out = ax::BinarizationMethod::Brink2Classes; return true; }
    if (s == "brink3")  { out = ax::BinarizationMethod::Brink3Classes; return true; }
    if (s == "sauvola") { out = ax::BinarizationMethod::Sauvola; return true; }
    if (s == "fixed")   { out = ax::BinarizationMethod::FixedAt127; return true; }
    return false;
}

std::string basename_no_ext(const std::string& path) {
    auto slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    auto dot = name.find_last_of('.');
    return (dot == std::string::npos) ? name : name.substr(0, dot);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr,
                     "usage: %s <input.jpg> <brink|brink3|sauvola|fixed> <output_dir>\n",
                     argv[0]);
        return 2;
    }
    const std::string input = argv[1];
    const std::string method_name = argv[2];
    const std::string output_dir = argv[3];

    ax::BinarizationMethod method;
    if (!parse_method(method_name, method)) {
        std::fprintf(stderr, "unknown method: %s\n", method_name.c_str());
        return 2;
    }

    cv::Mat gray = cv::imread(input, cv::IMREAD_GRAYSCALE);
    if (gray.empty()) {
        std::fprintf(stderr, "failed to load %s\n", input.c_str());
        return 1;
    }

    ax::BinarizeAndCleanParams params;
    params.method = method;
    // Use defaults for space_width / line_width. ImPage populates these
    // from staff geometry; goldens don't have that context, so we pin
    // to the defaults (1/1) which degrade the area-pruning formula to
    // a constant — deterministic and reproducible.

    cv::Mat binary;
    if (!ax::binarize_and_clean(gray, binary, params)) {
        std::fprintf(stderr, "binarize_and_clean failed\n");
        return 1;
    }

    std::uint64_t hash = fnv1a64(binary.data, binary.total());

    // Save visualization (0 -> 0, 1 -> 255) so the PNG is viewable.
    cv::Mat vis = binary * 255;
    std::string out_png = output_dir + "/" + basename_no_ext(input) + "_"
                        + method_name + ".png";
    if (!cv::imwrite(out_png, vis)) {
        std::fprintf(stderr, "failed to write %s\n", out_png.c_str());
        return 1;
    }

    std::printf("%s %016llx  %s\n", method_name.c_str(),
                (unsigned long long)hash, out_png.c_str());
    return 0;
}
