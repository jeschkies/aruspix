#ifndef AX_IMAGE_OPS_H
#define AX_IMAGE_OPS_H

#ifdef __cplusplus

#include <opencv2/core.hpp>

namespace ax {

// Paste `selection` into `image` at (pos_x, pos_y). The write is
// clipped against `image`'s bounds — negative offsets shift the
// source origin, oversize writes are truncated. `image` and
// `selection` must share the same channel count and element type.
// A no-op if the clipped write region collapses.
void set_data(cv::Mat& image, const cv::Mat& selection,
              int pos_x, int pos_y);

// Clip the axis-aligned rectangle (*pos_x, *pos_y, *width, *height)
// against the extent of `image`. Adjusts the four in/out parameters
// in place and returns true if the resulting rectangle is non-empty.
// Returns false and leaves the parameters untouched when the origin
// starts outside the image or the clipped size collapses.
bool safe_crop(const cv::Mat& image, int *width, int *height,
               int *pos_x, int *pos_y);

}  // namespace ax

#endif  // __cplusplus

#endif  // AX_IMAGE_OPS_H
