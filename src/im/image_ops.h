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

// Zero out connected components whose area is outside
// [min_area, max_area]. max_area == 0 means "no upper bound".
// Mirrors imProcessRemoveByArea's semantics: `src` is an 8-bit
// binary image (0/1 or 0/255), `dst` becomes the same image with
// out-of-range foreground components zeroed. Foreground values are
// preserved (whatever they were).
//
// `connectivity` matches OpenCV: 4 or 8.
void remove_by_area(const cv::Mat& src, cv::Mat& dst, int connectivity,
                    int min_area, int max_area = 0);

// Compute the bounding-box size of a (width x height) rectangle
// rotated by an angle whose cosine/sine are `cos0` / `sin0`. Matches
// imProcessCalcRotateSize's convention (samples the four corner
// pixel-centres via a rotate_transf that offsets by 0.5 and 1-pixel
// padding is added via `+ 2.0` before truncation).
void calc_rotate_size(int width, int height, int *new_width, int *new_height,
                      double cos0, double sin0);

}  // namespace ax

#endif  // __cplusplus

#endif  // AX_IMAGE_OPS_H
