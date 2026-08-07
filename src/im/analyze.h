#ifndef AX_ANALYZE_H
#define AX_ANALYZE_H

#ifdef __cplusplus

#include <opencv2/core.hpp>
#include <vector>

namespace ax {

// Runs analysis. Scans `src` in the given direction and looks for
// maximal runs of `type` (0 or 1). Reports the most frequent run
// length via `peak_val` and the median run length via `median_val`.
//
// `src` 8-bit single-channel with values in {0, 1}.
//
// Note: preserves a subtle off-by-one in the original imAnalyzeRuns
// where a K-pixel run is reported as length K-1. Callers already
// compensate; changing it would silently shift downstream heuristics.
void analyze_runs(const cv::Mat& src, int& peak_val, int& median_val,
                  int type = 0, bool vertical = true);

// Horizontal projection: hist[y] = sum of row y's pixel values.
// `src` 8-bit single-channel. `hist` is resized to src.rows.
void projection_h(const cv::Mat& src, std::vector<int>& hist);

// Vertical projection: hist[x] = sum of column x's pixel values.
// `src` 8-bit single-channel. `hist` is resized to src.cols.
void projection_v(const cv::Mat& src, std::vector<int>& hist);

}  // namespace ax

#endif  // __cplusplus

#endif  // AX_ANALYZE_H
