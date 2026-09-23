/////////////////////////////////////////////////////////////////////////////
// Name:        im_ext.cpp
// Author:      Laurent Pugin
// Created:     2005
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////
// Numeric helpers + ax:: image-op leaf functions previously bridged via
// the IM library. The IM-facing shims (imSetData, imProcessSafeCrop,
// imAnalyzeRuns, imProcess{Sauvola,Kittler}Threshold, imPhotogrammetric, …)
// have been removed; call sites use the ax:: replacements directly.
/////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>

using std::min;
using std::max;

#include "imext.h"
#include "thresholds.h"
#include "analyze.h"
#include "image_ops.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace ax {

void set_data(cv::Mat& image, const cv::Mat& selection,
              int pos_x, int pos_y)
{
	if (image.empty() || selection.empty()) return;
	if (image.type() != selection.type()) return;

	int w = selection.cols;
	int h = selection.rows;
	int sel_pos_x = 0;
	int sel_pos_y = 0;

	if ((pos_x > image.cols) || (pos_y > image.rows)) return;

	if (pos_x < 0) { w += pos_x; sel_pos_x = -pos_x; pos_x = 0; }
	if (pos_y < 0) { h += pos_y; sel_pos_y = -pos_y; pos_y = 0; }

	if (pos_x + w > image.cols) w = image.cols - pos_x;
	if (pos_y + h > image.rows) h = image.rows - pos_y;

	if ((w <= 0) || (h <= 0)) return;

	cv::Rect dst_rect(pos_x, pos_y, w, h);
	cv::Rect src_rect(sel_pos_x, sel_pos_y, w, h);
	selection(src_rect).copyTo(image(dst_rect));
}

bool safe_crop(const cv::Mat& image, int *width, int *height,
               int *pos_x, int *pos_y)
{
	int x = *pos_x;
	int y = *pos_y;
	int w = *width;
	int h = *height;

	if ((x > image.cols) || (y > image.rows)) return false;

	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }

	if (x + w > image.cols) w = image.cols - x;
	if (y + h > image.rows) h = image.rows - y;

	if ((w <= 0) || (h <= 0)) return false;

	*pos_x = x;
	*pos_y = y;
	*width = w;
	*height = h;
	return true;
}

void remove_by_area(const cv::Mat& src, cv::Mat& dst, int connectivity,
                    int min_area, int max_area)
{
	if (src.empty() || src.type() != CV_8UC1) {
		dst = src.clone();
		return;
	}
	// Threshold to a 0/255 mask for connectedComponentsWithStats — it
	// expects any non-zero value as foreground, but we normalize so the
	// output preserves whatever foreground value (0/1 or 0/255) `src`
	// used. We copy src into dst first, then zero the doomed labels.
	if (&dst != &src) dst = src.clone();

	cv::Mat labels, stats, centroids;
	int n = cv::connectedComponentsWithStats(src, labels, stats, centroids,
	                                          connectivity, CV_32S);
	for (int label = 1; label < n; ++label) {
		int area = stats.at<int>(label, cv::CC_STAT_AREA);
		bool too_small = area < min_area;
		bool too_big   = max_area > 0 && area > max_area;
		if (too_small || too_big) {
			cv::Mat mask = (labels == label);
			dst.setTo(0, mask);
		}
	}
}

void bit_plane_extract(const cv::Mat& src, cv::Mat& dst, int plane)
{
	if (src.empty() || src.type() != CV_8UC1) return;
	dst.create(src.rows, src.cols, CV_8UC1);
	const uchar mask = (uchar)(1 << plane);
	for (int y = 0; y < src.rows; ++y) {
		const uchar* s = src.ptr<uchar>(y);
		uchar*       d = dst.ptr<uchar>(y);
		for (int x = 0; x < src.cols; ++x)
			d[x] = (s[x] & mask) ? 1 : 0;
	}
}

void bit_plane_reset(cv::Mat& image, int plane)
{
	if (image.empty() || image.type() != CV_8UC1) return;
	cv::bitwise_and(image, cv::Scalar((uchar)~(1 << plane)), image);
}

void fill_holes(const cv::Mat& src, cv::Mat& dst, int connectivity)
{
	// Add a 1-pixel border of 0 (background) so cv::floodFill from a corner
	// always starts on a background pixel that transitively touches the
	// entire outer boundary. Fill the reachable background with a marker
	// value (2), then any remaining 0's are enclosed holes: promote them
	// to foreground.
	cv::Mat padded;
	cv::copyMakeBorder(src, padded, 1, 1, 1, 1,
	                   cv::BORDER_CONSTANT, cv::Scalar(0));
	cv::floodFill(padded, cv::Point(0, 0), cv::Scalar(2),
	              nullptr, cv::Scalar(), cv::Scalar(), connectivity);

	dst.create(src.rows, src.cols, CV_8UC1);
	for (int y = 0; y < src.rows; ++y) {
		const uchar* s = padded.ptr<uchar>(y + 1) + 1;
		uchar* d = dst.ptr<uchar>(y);
		for (int x = 0; x < src.cols; ++x)
			d[x] = (s[x] == 2) ? 0 : 1;
	}
}

void rotate_center(const cv::Mat& src, cv::Mat& dst,
                   int new_w, int new_h,
                   double cos0, double sin0, int order)
{
	// IM's imProcessRotate rotates around the source image centre and
	// places the rotated content centred in an output of size (new_w, new_h).
	// cv::getRotationMatrix2D produces the same M_IM = [cos sin; -sin cos]
	// convention as an inverse (dst -> src) map, which is exactly what
	// cv::warpAffine expects by default.
	// CV_PI is defined by opencv2/core.hpp and works on MSVC where M_PI
	// requires _USE_MATH_DEFINES.
	const double angle_deg = std::atan2(sin0, cos0) * 180.0 / CV_PI;
	cv::Point2f center(src.cols / 2.0f, src.rows / 2.0f);
	cv::Mat M = cv::getRotationMatrix2D(center, angle_deg, 1.0);
	M.at<double>(0, 2) += (new_w - src.cols) / 2.0;
	M.at<double>(1, 2) += (new_h - src.rows) / 2.0;
	const int interp = (order <= 0) ? cv::INTER_NEAREST
	                 : (order == 1) ? cv::INTER_LINEAR
	                 :                cv::INTER_CUBIC;
	cv::warpAffine(src, dst, M, cv::Size(new_w, new_h),
	               interp, cv::BORDER_CONSTANT, cv::Scalar(0));
}

void calc_rotate_size(int width, int height, int *new_width, int *new_height,
                      double cos0, double sin0)
{
	// Port of imProcessCalcRotateSize (IM's src/process/im_geometric.cpp).
	// Sample the four corner pixel-centres (+0.5) around the image midpoint,
	// rotate each, then take the axis-aligned bounding box + 1-pixel pad.
	const double wd2 = double(width) / 2.0;
	const double hd2 = double(height) / 2.0;

	auto rotate_transf = [&](int x, int y, double &xl, double &yl) {
		double xr = x + 0.5 - wd2;
		double yr = y + 0.5 - hd2;
		xl = ( xr * cos0 + yr * sin0);
		yl = (-xr * sin0 + yr * cos0);
	};

	double xl, yl;
	rotate_transf(0, 0, xl, yl);
	double xmin = xl, xmax = xl, ymin = yl, ymax = yl;

	auto sample = [&](int x, int y) {
		rotate_transf(x, y, xl, yl);
		if (xl < xmin) xmin = xl; if (xl > xmax) xmax = xl;
		if (yl < ymin) ymin = yl; if (yl > ymax) ymax = yl;
	};
	sample(width - 1, height - 1);
	sample(0,         height - 1);
	sample(width - 1, 0);

	*new_width  = (int)(xmax - xmin + 2.0);
	*new_height = (int)(ymax - ymin + 2.0);
}

}  // namespace ax


/* function that calculate median of an array with bubble sort algorithm */
int median( int a[], int size, bool sort_array )
{
	if ( !a || ( size==0 ) )
		return 0;

   //bubbleSort
   int i, j, tmp;

   if ( !sort_array )
   {
	   int *b = new int[ size ];
	   memcpy( b, a, size * sizeof(int) );
	   a = b;
   }

   for ( i = 1; i < size; i++ ) {
      for ( j = 0; j < size - 1; j++ ) {
         if ( a[ j ] > a[ j + 1 ] ) {
            tmp = a[ j ];
            a[ j ] = a[ j + 1 ];
            a[ j + 1 ] = tmp;
         }
      }
   }
   int med_value;
   if ( ( size % 2 ) == 0)
	   med_value = ( a[ (size - 1)/2 ] + a[ (size + 1)/2 ] ) / 2;
   else
	   med_value =  a[ size / 2 ];

   if ( !sort_array )
		delete[] a;

   return med_value;
}


float medianf( float a[], int size, bool sort_array )
{
	if ( !a || ( size==0 ) )
		return 0;

   //bubbleSort
   int i, j;
   float tmp;

   if ( !sort_array )
   {
	   float *b = new float[ size ];
	   memcpy( b, a, size * sizeof(float) );
	   a = b;
   }

   for ( i = 1; i < size; i++ ) {
      for ( j = 0; j < size - 1; j++ ) {
         if ( a[ j ] > a[ j + 1 ] ) {
            tmp = a[ j ];
            a[ j ] = a[ j + 1 ];
            a[ j + 1 ] = tmp;
         }
      }
   }
   float med_value;
   if ( ( size % 2 ) == 0)
	   med_value = ( a[ (size - 1)/2 ] + a[ (size + 1)/2 ] ) / 2;
   else
	   med_value =  a[ size / 2 ];

   if ( !sort_array )
		delete[] a;

   return med_value;
}

int max_val( int a[], int size , int *pos )
{
	if (pos)
		*pos = 0;

	if ( !a || ( size==0 ) )
		return 0;

	int max = 0;
	int p = 0;

	for (int  i = 0; i < size; i++ )
	{
		if ( a[i] >= max )
		{
			max = a[i];
			p = i;
		}
	}

	if (pos)
		*pos = p;

	return max;
}


int sum( int a[], int size )
{
	if ( !a || ( size==0 ) )
		return 0;

	int sum = 0;
	for (int  i = 0; i < size; i++ )
		sum  += a[i];

	return sum;
}


int count( int a[], int size )
{
	if ( !a || ( size==0 ) )
		return 0;

	int count = 0;
	for (int  i = 0; i < size; i++ )
		if ( a[i] )
			count++;

	return count;
}


void corr( int a[], int b[], int size, int win, int *dec, int *max)
{
	if ( !a || !b || !dec || !max )
		return;

	int pad = size + 2 * win;
	int *c = new int[ pad ];
	memset( c, 0, pad * sizeof(int));
	memcpy( c + win, a, size * sizeof(int) );

    int conv_width = 2 * win;
	int *mask = new int[ size ];

    int max_val = 0, max_pos = 0;
    for (int x = 0; x < conv_width; x++)
    {
		memcpy( mask, c + x, size * sizeof(int) );
        int sum = 0;
        for (int i = 0; i < size; i++)
        {
			sum += mask[i] * b[i];
        }
		if (sum > max_val)
        {
			max_val = sum;
            max_pos = x;
        }
    }

    *dec = max_pos - win;
	*max = max_val;
	if ( max_val == 0 )
		*dec = 0;
    delete[] c;
	delete[] mask;
}


/*
	Analyse les runs dans une image b/w
	peak_val est la longueur du run le plus represente dans l'image
	median_val est la longueur median de tous les runs
 */
namespace ax {

void analyze_runs(const cv::Mat& src, int& peak_val, int& median_val,
                  int type, bool vertical)
{
	if (src.type() != CV_8UC1) {
		peak_val = 0;
		median_val = 0;
		return;
	}
	const std::uint8_t *bufIm = src.data;
	int h, w;
	if ( vertical )
	{
		h = src.rows;
		w = src.cols;
	}
	else
	{
		w = src.rows;
		h = src.cols;
	}

	// runs
	int* runs = (int*)malloc( h * w * sizeof(int) );
	memset(runs, 0, h * w * sizeof(int) );

	// tableau compter les runs de chaque longueur (pour touver peak)
	int* vals = (int*)malloc( h * sizeof(int) );
	memset(vals, 0, h * sizeof(int) );

    int x, y;
	int run_type, run_val;
	int i = 0;

    for (x = 0; x < w; x++)
    {
        run_type = 0;
        run_val = 0;
        for (y = 0; y < h; y++)
        {
            int offset;

			if (vertical)
				offset = y * w + x;
			else
				offset = x * h + y;

            if ( bufIm[ offset ] == run_type )
                run_val++;
            else // changement
            {
                if ( ( run_type == type ) && ( run_val > 0 )  ) // run recherche
                {
                    runs[i] = run_val;
					vals[run_val] = vals[run_val] + 1;
                    i++;
                }
                run_type = ( run_type == 1 ) ? 0 : 1 ;
                run_val = 0;
            }
        }
    }

	if ( i > 0 )
	{
		max_val( vals, h, &peak_val );
		median_val = median( runs, i, false );
	}
	else
	{
		peak_val = 0;
		median_val = 0;
	}

	free( runs );
	free( vals );
}

}  // namespace ax

/*
	Calcule la projection horizontale d'une image
	hist doit avoir la taille de la hauteur de l'image
 */
namespace ax {

void projection_h(const cv::Mat& src, std::vector<int>& hist)
{
	hist.assign(src.rows, 0);
	if (src.type() != CV_8UC1) return;
	for (int y = 0; y < src.rows; ++y) {
		const std::uint8_t *row = src.ptr<std::uint8_t>(y);
		int hist_val = 0;
		for (int x = 0; x < src.cols; ++x)
			hist_val += row[x];
		hist[y] = hist_val;
	}
}

void projection_v(const cv::Mat& src, std::vector<int>& hist)
{
	hist.assign(src.cols, 0);
	if (src.type() != CV_8UC1) return;
	for (int y = 0; y < src.rows; ++y) {
		const std::uint8_t *row = src.ptr<std::uint8_t>(y);
		for (int x = 0; x < src.cols; ++x)
			hist[x] += row[x];
	}
}

}  // namespace ax


/*
	Supprime les element dont la hauteur moyenne n'est pas entre min et max
	Si max = 0, le seuil superieur est ignore
	*image est une image labelisee (bg = 0, puis 1,2 ...)
	region_count est le nombre de regions
 */
namespace ax {

void clear_height(cv::Mat& src, int region_count,
                  int min_threshold, int max_threshold)
{
	if (src.type() != CV_16UC1 || region_count <= 0) return;

	std::vector<int> heights(src.cols * region_count, 0);
	const int count = src.rows * src.cols;
	std::uint16_t *img_data = src.ptr<std::uint16_t>();
	for (int i = 0; i < count; ++i) {
		if (img_data[i])
			heights[ (img_data[i] - 1) * src.cols + i % src.cols ]++;
	}
	for (int i = 0; i < count; ++i) {
		if (img_data[i]) {
			int h = heights[ (img_data[i] - 1) * src.cols + i % src.cols ];
			if (h < min_threshold)
				img_data[i] = 0;
			else if (max_threshold && h > max_threshold)
				img_data[i] = 0;
		}
	}
}

}  // namespace ax


/*
	Supprime les element dont la largeur ou la hauteur maximal est plus petite que threshold
	*image est une image labelisee (bg = 0, puis 1,2 ...)
	region_count est le nombre de regions
 */
namespace ax {

void clear_min(cv::Mat& src, int region_count, int threshold)
{
	if (src.type() != CV_16UC1 || region_count <= 0) return;

	std::vector<int> boxes;
	bounding_boxes(src, boxes, region_count);

	const int count = src.rows * src.cols;
	std::uint16_t *img_data = src.ptr<std::uint16_t>();
	for (int i = 0; i < count; ++i) {
		if (img_data[i]) {
			int j = (img_data[i] - 1) * 4;
			if ((boxes[j+1] - boxes[j+0] < threshold) ||
			    (boxes[j+3] - boxes[j+2] < threshold))
				img_data[i] = 0;
		}
	}
}

}  // namespace ax

/*
	Supprime les element dont la largeur moyenne n'est pas entre min et max
	Si max = 0, le seuil superieur est ignore
	*image est une image labelisee (bg = 0, puis 1,2 ...)
	region_count est le nombre de regions
 */
namespace ax {

void clear_width(cv::Mat& src, int region_count,
                 int min_threshold, int max_threshold)
{
	if (src.type() != CV_16UC1 || region_count <= 0) return;

	std::vector<int> widths(src.rows * region_count, 0);
	const int count = src.rows * src.cols;
	std::uint16_t *img_data = src.ptr<std::uint16_t>();
	for (int i = 0; i < count; ++i) {
		if (img_data[i])
			widths[ (img_data[i] - 1) * src.rows + i / src.cols ]++;
	}
	for (int i = 0; i < count; ++i) {
		if (img_data[i]) {
			int w = widths[ (img_data[i] - 1) * src.rows + i / src.cols ];
			if (w < min_threshold)
				img_data[i] = 0;
			else if (max_threshold && w > max_threshold)
				img_data[i] = 0;
		}
	}
}

}  // namespace ax

/*
	calcule les bounding boxes pour chaque label
	*image est une image labelisee (bg = 0, puis 1,2 ...)
	region_count est le nombre de regions
	boxes est tableau des bounding boxes 4 * region_count : pour chaque region xmin xmax ymin ymax
 */
namespace ax {

void bounding_boxes(const cv::Mat& src, std::vector<int>& boxes,
                    int region_count)
{
	boxes.assign(4 * region_count, 0);
	if (src.type() != CV_16UC1 || region_count <= 0) return;

	for (int i = 0; i < region_count; ++i) {
		boxes[4 * i + 0] = src.cols;
		boxes[4 * i + 2] = src.rows;
	}

	const int count = src.rows * src.cols;
	const std::uint16_t *img_data = src.ptr<std::uint16_t>();
	for (int i = 0; i < count; ++i) {
		if (img_data[i]) {
			int idx = (img_data[i] - 1) * 4;
			int x = i % src.cols;
			int y = i / src.cols;
			if (boxes[idx + 0] > x)      boxes[idx + 0] = x;
			else if (boxes[idx + 1] < x) boxes[idx + 1] = x;
			if (boxes[idx + 2] > y)      boxes[idx + 2] = y;
			else if (boxes[idx + 3] < y) boxes[idx + 3] = y;
		}
	}
}

}  // namespace ax

static unsigned char Kittler(const cv::Mat& src, double *mu_1, double *mu_2, double *mu)
{
  unsigned long h[256];
  int threshold;
  double criterion;
  int g;
  int n;
  int T_low, T_high;
  int P_1_T, P_2_T, P_tot;
  double mu_1_T, mu_2_T;
  double sum_gh_1, sum_gh_2, sum_gh_tot;
  double sum_ggh_1, sum_ggh_2, sum_ggh_tot;
  double sigma_1_T, sigma_2_T;
  double J_T;

  {
    int histSize = 256;
    float range[] = {0.0f, 256.0f};
    const float *histRange = range;
    cv::Mat histMat;
    cv::calcHist(&src, 1, /*channels=*/nullptr, cv::Mat(),
                 histMat, 1, &histSize, &histRange);
    for (int i = 0; i < 256; ++i)
      h[i] = static_cast<unsigned long>(histMat.at<float>(i));
  }

  criterion = 1e10;
  threshold = 127;
  J_T = criterion;

  T_low = 0;
  while((h[T_low] == 0) && (T_low < 255))
    T_low++;

  T_high = 255;
  while((h[T_high] == 0) && (T_high > 0))
    T_high--;

  n = 0;
  for (g=T_low; g<=T_high; g++)
    n += h[g];

  P_1_T = h[T_low];
  P_tot = 0;
  for (g=T_low; g<= T_high; g++)
    P_tot += h[g];

  sum_gh_1 = T_low * h[T_low];
  sum_gh_tot = 0.0;
  for (g=T_low; g<=T_high; g++)
    sum_gh_tot += g*h[g];

  *mu = sum_gh_tot * 1.0 / n;

  sum_ggh_1 = T_low*T_low*h[T_low];
  sum_ggh_tot = 0.0;
  for (g=T_low; g<=T_high; g++)
    sum_ggh_tot += g*g*h[g];

  for (g=T_low+1; g<T_high-1; g++)
    {
      P_1_T += h[g];
      P_2_T = P_tot - P_1_T;

      sum_gh_1 += g*h[g];
      sum_gh_2 = sum_gh_tot - sum_gh_1;

      mu_1_T = sum_gh_1 / P_1_T;
      mu_2_T = sum_gh_2 / P_2_T;

      sum_ggh_1 += g*g*h[g];
      sum_ggh_2 = sum_ggh_tot - sum_ggh_1;

      sigma_1_T = sum_ggh_1/P_1_T - mu_1_T * mu_1_T;
      sigma_2_T = sum_ggh_2/P_2_T - mu_2_T * mu_2_T;

      /* Equation (15) in the article */
      if ((sigma_1_T != 0.0) && (P_1_T != 0) &&
	  (sigma_2_T != 0.0) && (P_2_T != 0))
	J_T = 1 + 2*(P_1_T*log(sigma_1_T) + P_2_T*log(sigma_2_T))
	  - 2*(P_1_T*log((double)P_1_T) + P_2_T*log((double)P_2_T) );

      if (criterion > J_T)
	{
	  criterion = J_T;
	  threshold = g;
	  *mu_1 = mu_1_T;
	  *mu_2 = mu_2_T;
	}
    }
  return threshold;
}

namespace ax {

int sauvola_threshold(const cv::Mat& src_in, cv::Mat& dst, int region_size,
                      float sensitivity, int dynamic_range,
                      int lower_bound, int upper_bound, bool white_is_255)
{
	if ((region_size < 1) || (region_size > std::min(src_in.cols, src_in.rows)))
		return 0;
	if (src_in.type() != CV_8UC1)
		return 0;

	// Local mean / stddev via O(1)-per-pixel box filters (the previous
	// IM-based implementation called imProcessCrop + imCalcImageStatistics
	// once per output pixel — orders of magnitude slower).
	cv::Mat src = src_in.clone();
	if (!white_is_255) src = 255 - src;
	cv::Mat src32f;
	src.convertTo(src32f, CV_32F);

	cv::Size kernel(region_size, region_size);
	cv::Mat means, mean_of_sq, variance, stddev;
	cv::boxFilter(src32f, means, CV_32F, kernel,
	              cv::Point(-1, -1), /*normalize=*/true,
	              cv::BORDER_REPLICATE);
	cv::sqrBoxFilter(src32f, mean_of_sq, CV_32F, kernel,
	                 cv::Point(-1, -1), /*normalize=*/true,
	                 cv::BORDER_REPLICATE);
	variance = mean_of_sq - means.mul(means);
	cv::max(variance, 0.0, variance);
	cv::sqrt(variance, stddev);

	dst.create(src.rows, src.cols, CV_8UC1);
	for (int y = 0; y < src.rows; ++y) {
		const uchar *src_row = src.ptr<uchar>(y);
		const float *mean_row = means.ptr<float>(y);
		const float *std_row = stddev.ptr<float>(y);
		uchar *dst_row = dst.ptr<uchar>(y);
		for (int x = 0; x < src.cols; ++x) {
			int pixel_value = src_row[x];
			if (pixel_value < lower_bound) {
				dst_row[x] = 1;  // black
			} else if (pixel_value >= upper_bound) {
				dst_row[x] = 0;  // white
			} else {
				float adjusted_deviation =
				    std_row[x] / (float)dynamic_range - 1.0f;
				float threshold =
				    mean_row[x] + (1.0f + sensitivity * adjusted_deviation);
				dst_row[x] = (pixel_value > threshold) ? 0 : 1;
			}
		}
	}
	return 1;
}

}  // namespace ax

namespace ax {

int kittler_threshold(const cv::Mat& src, cv::Mat& dst)
{
  if (src.type() != CV_8UC1) return 0;
  double dummy_1, dummy_2, dummy_3;
  int level = ::Kittler(src, &dummy_1, &dummy_2, &dummy_3);
  dst.create(src.rows, src.cols, CV_8UC1);
  // imProcessThreshold semantics: dst = (src <= level) ? 0 : 1.
  // cv::threshold with THRESH_BINARY: dst = (src > thresh) ? maxval : 0.
  // Same behavior with thresh=level, maxval=1.
  cv::threshold(src, dst, level, 1, cv::THRESH_BINARY);
  return level;
}

}  // namespace ax
