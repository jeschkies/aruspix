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

// Compute bounding boxes of a labeled 16-bit image. Label 0 is
// background; labels 1..region_count are the foreground regions.
// `boxes` is resized to 4*region_count and populated as
//   [xmin, xmax, ymin, ymax]  per region, in label order.
// Note: xmax/ymax are the pixel indices, not spans — a K-wide
// region reports xmax-xmin = K-1. Callers that want cv::Rect
// should add +1 to the span.
void bounding_boxes(const cv::Mat& src, std::vector<int>& boxes,
                    int region_count);

// Zero out labels in a 16-bit labeled image whose bounding box
// is narrower or shorter than `threshold`.
void clear_min(cv::Mat& src, int region_count, int threshold);

// Zero out per-pixel: for each labeled pixel, count how many pixels
// share its (label, column). If that count is < min_threshold or,
// when max_threshold != 0, > max_threshold, zero the pixel. Same for
// clear_width but per-row.
void clear_height(cv::Mat& src, int region_count,
                  int min_threshold, int max_threshold);
void clear_width(cv::Mat& src, int region_count,
                 int min_threshold, int max_threshold);

}  // namespace ax

#endif  // __cplusplus

#endif  // AX_ANALYZE_H
