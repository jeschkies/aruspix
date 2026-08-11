/////////////////////////////////////////////////////////////////////////////
// Name:        im_ext.cpp
// Author:      Laurent Pugin
// Created:     2005
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////
// Extenstion de IM_LIB
/////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <math.h>
#include <memory.h>
#include <stdio.h>

using std::min;
using std::max;

#include "imext.h"
#include "thresholds.h"
#include "analyze.h"
#include "image_ops.h"

#include <im.h>
#include <im_image.h>
#include <im_convert.h>
#include <im_process.h>
#include <im_util.h>
#include <im_binfile.h>
#include <im_counter.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace {

// Wrap an imImage's first plane as an in-place cv::Mat view (no copy).
// Currently we only need the IM_BYTE / IM_USHORT cases used by the
// thresholding algorithms below; extend as more functions are migrated.
inline cv::Mat as_mat(_imImage *image) {
    int cv_type = (image->data_type == IM_USHORT) ? CV_16UC1 : CV_8UC1;
    return cv::Mat(image->height, image->width, cv_type, image->data[0]);
}
inline cv::Mat as_mat(const _imImage *image) {
    int cv_type = (image->data_type == IM_USHORT) ? CV_16UC1 : CV_8UC1;
    // cv::Mat's data pointer is non-const, but imgproc treats input
    // mats as const where it matters. The const_cast here mirrors how
    // OpenCV's own InputArray adapters work.
    return cv::Mat(image->height, image->width, cv_type,
                   const_cast<void *>(image->data[0]));
}

}  // namespace

// Function taken from im_convolve_rank.cpp in imlib
template <class T, class DT> 
static int DoConvolveRankFunc(T *map, DT* new_map, int width, int height, int kw, int kh, DT (*func)(T* value, int count, int center), int counter)
{
  T* value = new T[kw*kh];
  int offset, new_offset, i, j, x, y, v, c;
  int kh1, kw1, kh2, kw2;

  kh2 = kh/2;
  kw2 = kw/2;
  kh1 = -kh2;
  kw1 = -kw2;
  if (kh%2==0) kh2--;  // if not odd decrease 1
  if (kw%2==0) kw2--;

  for(j = 0; j < height; j++)
  {
    new_offset = j * width;

    for(i = 0; i < width; i++)
    {
      v = 0; c = 0;
    
      for(y = kh1; y <= kh2; y++)
      {
        if ((j + y < 0) ||        // pass the bottom border
            (j + y >= height))    // pass the top border
          continue;

        offset = (j + y) * width;

        for(x = kw1; x <= kw2; x++)
        {
          if ((i + x < 0) ||      // pass the left border
              (i + x >= width))   // pass the right border
            continue;

          if (x == 0 && y == 0)
            c = v;

          value[v] = map[offset + (i + x)];
          v++;
        }
      }
      
      new_map[new_offset + i] = (DT)func(value, v, c);
    }    

    if (!imCounterInc(counter))
    {
      delete[] value;
      return 0;
    }
  }

  delete[] value;
  return 1;
}


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

// Delegate to ax::set_data. imImage carries a depth (per-plane count)
// which cv::Mat expresses via channels; we iterate planes here for
// the (rare) multi-plane case, wrapping each in an 8-bit view because
// imSetData never inspected the element type beyond byte width.
void imSetData( _imImage *image, _imImage *selection, int pos_x, int pos_y )
{
	if (image->depth != selection->depth) return;
	int type_size = imDataTypeSize(image->data_type);
	int row_stride_bytes = image->width * type_size;
	// Wrap each plane as an 8-bit Mat sized (height, width * type_size)
	// so ax::set_data's copyTo works byte-for-byte, matching imSetData's
	// original memcpy loop irrespective of the imImage element type.
	for (int i = 0; i < image->depth; ++i) {
		cv::Mat img_plane(image->height, image->width * type_size, CV_8UC1,
		                  image->data[i]);
		cv::Mat sel_plane(selection->height, selection->width * type_size,
		                  CV_8UC1, selection->data[i]);
		ax::set_data(img_plane, sel_plane, pos_x * type_size, pos_y);
	}
}

bool imProcessSafeCrop( _imImage *image, int *width, int *height, int *pos_x, int *pos_y )
{
	cv::Mat src(image->height, image->width, CV_8UC1,
	            const_cast<void*>(image->data[0]));
	return ax::safe_crop(src, width, height, pos_x, pos_y);
}


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

double** alloc2DArray( int x, int y )
{
    double** array;  
    array = (double**) malloc(x*sizeof(double*));  
    for (int i = 0; i < x; i++)  
        array[i] = (double*) malloc(y*sizeof(double));  
    return array;  
} 

void free2DArray( double **array, int x )
{
    int i;
    for (i = 0; i < x; i++){  
        free(array[i]);  
    }  
    free(array); 
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
	const imbyte *bufIm = src.data;
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

void imAnalyzeRuns(const imImage* image, int *peak_val, int *median_val, int type, bool vertical)
{
	cv::Mat src(image->height, image->width, CV_8UC1,
	            const_cast<void*>(image->data[0]));
	ax::analyze_runs(src, *peak_val, *median_val, type, vertical);
}

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
		const imbyte *row = src.ptr<imbyte>(y);
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
		const imbyte *row = src.ptr<imbyte>(y);
		for (int x = 0; x < src.cols; ++x)
			hist[x] += row[x];
	}
}

}  // namespace ax

void imAnalyzeProjectionH(const imImage* image, int* hist)
{
	cv::Mat src(image->height, image->width, CV_8UC1,
	            const_cast<void*>(image->data[0]));
	std::vector<int> tmp;
	ax::projection_h(src, tmp);
	std::copy(tmp.begin(), tmp.end(), hist);
}

/*
	Calcule la projection verticale d'une image
	hist doit avoir la taille de la largeur de l'image
 */
void imAnalyzeProjectionV(const imImage* image, int* hist)
{
	cv::Mat src(image->height, image->width, CV_8UC1,
	            const_cast<void*>(image->data[0]));
	std::vector<int> tmp;
	ax::projection_v(src, tmp);
	std::copy(tmp.begin(), tmp.end(), hist);
}


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
	imushort *img_data = src.ptr<imushort>();
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

void imAnalyzeClearHeight(const imImage* image, int region_count, int min_threshold, int max_threshold )
{
	cv::Mat src(image->height, image->width, CV_16UC1, image->data[0]);
	ax::clear_height(src, region_count, min_threshold, max_threshold);
}


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
	imushort *img_data = src.ptr<imushort>();
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

void imAnalyzeClearMin(const imImage* image, int region_count, int threshold )
{
	cv::Mat src(image->height, image->width, CV_16UC1, image->data[0]);
	ax::clear_min(src, region_count, threshold);
}

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
	imushort *img_data = src.ptr<imushort>();
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

void imAnalyzeClearWidth(const imImage* image, int region_count, int min_threshold, int max_threshold )
{
	cv::Mat src(image->height, image->width, CV_16UC1, image->data[0]);
	ax::clear_width(src, region_count, min_threshold, max_threshold);
}

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
	const imushort *img_data = src.ptr<imushort>();
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

void imAnalyzeBoundingBoxes(const imImage* image, int* boxes, int region_count )
{
	cv::Mat src(image->height, image->width, CV_16UC1,
	            const_cast<void*>(image->data[0]));
	std::vector<int> tmp;
	ax::bounding_boxes(src, tmp, region_count);
	std::copy(tmp.begin(), tmp.end(), boxes);
}

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

enum {
	IM_RANGE_MEAN,
	IM_RANGE_STDDEV
};


static float mean_op_byte(imbyte* value, int count, int center)
{
	float mean;
	for (int i = 0; i < count; i++)
		mean += (float)value[i];

	mean /= float(count);
	return mean;
}

static float stddev_op_byte(imbyte* value, int count, int center)
{
	float stddev;
	float mean;
	for (int i = 0; i < count; i++)
	{
		stddev += ((float)value[i])*((float)value[i]);
		mean += (float)value[i];
	}

	mean /= float(count);
	stddev = (float)sqrt((stddev - count * mean*mean)/(count-1.0));
	return stddev;
}

/*
	Uses DoConvolveRankFunc from im_convovle_rank
*/
int imProcessRange(const imImage* src_image, imImage* dst_image, int ks, int op_type )
{
	int ret = 0;
	int counter = imCounterBegin("Range Mean");
	imCounterTotal(counter, src_image->depth*src_image->height, "Filtering...");

	switch( op_type )
	{
	case IM_RANGE_MEAN:
		ret = DoConvolveRankFunc((imbyte*)src_image->data[0], (float*)dst_image->data[0], 
                             src_image->width, src_image->height, ks, ks, mean_op_byte, counter);
		break;
	case IM_RANGE_STDDEV:
		ret = DoConvolveRankFunc((imbyte*)src_image->data[0], (float*)dst_image->data[0], 
                             src_image->width, src_image->height, ks, ks, stddev_op_byte, counter);
		break;
	}
							 
	imCounterEnd(counter);
	return ret;
}

/*
	implementation using imProcessRange with two float images
*/

/*
int imProcessSauvolaThreshold( const imImage* image, imImage* dest, int region_size,
	float sensitivity, int dynamic_range, int lower_bound, int upper_bound, bool white_is_255 )
{
    if ((region_size < 1) || (region_size > min(image->width, image->height)))
		return 0;
	
	imImage *src = imImageDuplicate( image );
     
	if ( !white_is_255 )
		imProcessNegative( src, src );

	imImage *im_means = imImageCreate( image->width, image->height, IM_GRAY, IM_FLOAT );
	imImage *im_std_dev = imImageCreate( image->width, image->height, IM_GRAY, IM_FLOAT );

	int ret = 1;
    // Compute regional statistics.
	if ( ret )
		ret = imProcessRange( src, im_means, region_size, IM_RANGE_MEAN );
	if ( ret )
		ret = imProcessRange( src, im_std_dev, region_size, IM_RANGE_STDDEV );
	
	int counter = imCounterBegin("Sauvola threshold");
	imCounterTotal(counter, src->height, "Sauvola threshold");

	imbyte* src_data = (imbyte*)src->data[0];
	imbyte* dest_data = (imbyte*)dest->data[0];
	float* means = (float*)im_means->data[0];
	float* std_dev = (float*)im_std_dev->data[0];
	
	
	int offset, pixel_value;
	float mean, deviation, adjusted_deviation, threshold;

    for (int y = 0; y < src->height; y++) {
		if ( !ret ) // aborted or error
			break; 
        for (int x = 0; x < src->width; x++) {
			offset = y * src->width + x;
			pixel_value = src_data[ offset ];
            // Check global thresholds and then threshold adaptively.
            if (pixel_value < lower_bound) {
                dest_data[ offset ] = 1; // black, 1 in the destination image
            } else if (pixel_value >= upper_bound) {
                dest_data[ offset ] = 0; // white
            } else {
                mean = means[ offset ];
                deviation = std_dev[ offset ];
                adjusted_deviation
                    = deviation / (float)dynamic_range - 1.0;
                threshold
                    = mean + (1.0 + sensitivity * adjusted_deviation);
                dest_data[ offset ] = (pixel_value > threshold) ? 0 : 1;
            }
        }
		ret = imCounterInc(counter);
    }
	imImageDestroy( src );
	imImageDestroy( im_means );
	imImageDestroy( im_std_dev );
	imCounterEnd( counter );
	return ret;
}
*/


int imMeanAndStdDevFilter(const imImage *image, int region_size, float *means, float *std_dev, int counter )
{
     if ((region_size < 1) || (region_size > min(image->width, image->height)))
		return 0;

    int half_region_size = region_size / 2;
	
	int ulx, uly, lrx, lry, offset;
	imImage *region;
	imStats stats;

    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            // Define the region.
			offset = y * image->width + x;
			ulx = max( 0, x - half_region_size);
			uly = max( 0, y - half_region_size);
			lrx = min( x + half_region_size, image->width - 1 );
			lry = min( y + half_region_size, image->height - 1 );
			region = imImageCreate( lrx - ulx, lry - uly, image->color_space, image->data_type ); 
			imProcessCrop( image, region, ulx, uly );  
            imCalcImageStatistics( region, &stats );
            means[ offset ] = stats.mean;
			std_dev[ offset ] = stats.stddev;
			imImageDestroy( region );
			if (!imCounterInc(counter))
				return 0;
        }
    }
	return 1;
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

int imProcessSauvolaThreshold( const imImage* image, imImage* dest, int region_size,
	float sensitivity, int dynamic_range, int lower_bound, int upper_bound, bool white_is_255 )
{
	cv::Mat dst = as_mat(dest);
	return ax::sauvola_threshold(as_mat(image), dst, region_size, sensitivity,
	                             dynamic_range, lower_bound, upper_bound,
	                             white_is_255);
}

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

int imProcessKittlerThreshold(const imImage* image, imImage* NewImage )
{
  cv::Mat dst = as_mat(NewImage);
  return ax::kittler_threshold(as_mat(image), dst);
}

void imPhotogrammetric( const imImage* image, imImage* dest ){
	
	int height = image->height;
	int width = image->width;
	
	if ( height % 2 != 0 ) height += 1;
	if ( width % 2 != 0 ) width += 1;

	imImage *imagetmp = imImageCreate(width, height, image->color_space, image->data_type);
	imProcessAddMargins( image, imagetmp, 0, 0 );
	
	int i, j;
	double *vk = (double*) malloc( height * sizeof(double) );
	double *vl = (double*) malloc( width * sizeof(double) );
	double **matk = (double**) malloc( height * sizeof(double*) );
	double **matl = (double**) malloc( height * sizeof(double*) );
	
	imbyte* X = (imbyte*)imagetmp->data[0];
	double sum_vk, sum_vl;
	double a = 0;
	double b = 0;
	double c = 0;
	double **X2 = (double**) malloc ( height * sizeof(double*) );
	double **X3 = (double**) malloc ( height * sizeof(double*) );
	
	for ( i = 0; i < height; i++ ){
		matk[i] = (double*) malloc ( width * sizeof(double) );
		matl[i] = (double*) malloc ( width * sizeof(double) );
		X2[i] = (double*) malloc ( width * sizeof(double) );
		X3[i] = (double*) malloc ( width * sizeof(double) );
	}
	
	for ( i = 0; i < (height / 2); i++ ){
		vk[i] = 0;
		double val = -(height / 2) + i;
		if ( val <= -1 ) vk[i] = val;					// vk(1:(height/2)) = [-(height/2):1:-1]	
	}
	for ( i = (height / 2) ; i < height; i++ ){
		vk[i] = 0;
		double val = 1 + i - (height / 2);
		if ( val <= (height / 2) ) vk[i] = val;			// vk((height/2)+1:height) = [1:1:(height/2)]	
	}
	
	for ( i = 0; i < (width / 2); i++ ){
		vl[i] = 0;
		double val = -(width / 2) + i;
		if ( val <= -1 ) vl[i] = val;					// vl(1:(width/2)) = [-(width/2):1:-1]	
	}
	for ( i = (width / 2) ; i < width; i++ ){
		vl[i] = 0.0;
		double val = 1 + i - (width / 2);
		if ( val <= (width / 2) ) vl[i] = val;			// vl((width/2)+1:width) = [1:1:(width/2)]	
	}
	
	for ( i = 0; i < width; i++ )
		for ( j = 0; j < height; j++ )
			matk[j][i] = vk[j];							// matk = repmat(vk, 1, width)
		 
	for ( i = 0; i < height; i++ )
		for ( j = 0; j < width; j++ )
			matl[i][j] = vl[j];							// matl = repmat(vl, height, 1)

	/*
	 Coordinates of the plane:
	 a=sum(sum(X.*matk))/(width*sum(vk.^2));
	 b=sum(sum(X.*matl))/(height*sum(vl.^2));
	 c=sum(sum(X))/(height*width);
	*/	
	
	for ( i = 0; i < height; i++ ) sum_vk += (vk[i] * vk[i]);		// sum(vk.^2)
	for ( i = 0; i < width; i++ ) sum_vl += (vl[i] * vl[i]);		// sum(vl.^2)
	
	for ( i = 0; i < height; i++ ){
		for ( j = 0; j < width; j++ ){
			int offset = i*width + j;
			a += ( (double) X[offset] * matk[i][j] );				// sum(sum(X.*matk))
			b += ( (double) X[offset] * matl[i][j] );				// sum(sum(X.*matl))
			c += X[offset];											// sum(sum(X))
		}
	}
	
	a /= ( width * sum_vk );										// a=sum(sum(X.*matk))/(width*sum(vk.^2))
	b /= ( height * sum_vl );										// b=sum(sum(X.*matl))/(height*sum(vl.^2))
	c /= ( height * width );										// c=sum(sum(X))/(height*width)
	for ( i = 0; i < height; i++ ){
		for ( j = 0; j < width; j++ ){
			X2[i][j] = (matk[i][j] * a) + (matl[i][j] * b) + c;		// X2=matk.*a+matl.*b+c
			X3[i][j] = (double) X[i*width + j] - X2[i][j];			// X3 = X - X2
		}
	}
		
	/* Normalize */
	double min = X3[0][0];
	for ( i = 0; i < height; i++ )
		for ( j = 0; j < width; j++ )
			if ( min > X3[i][j] ) min = X3[i][j];  // Find minimum
			
	for ( i = 0; i < height; i++ )
		for ( j = 0; j < width; j++ )
			X3[i][j] = X3[i][j] + abs(min);							// X3=X3+abs(min(min(X3)))
			
	double max = X3[0][0];
	for ( i = 0; i < height; i++ )
		for ( j = 0; j < width; j++ )
			if ( max < X3[i][j] ) max = X3[i][j];					// Find maximum
	
	for ( i = 0; i < height; i++ ){
		for ( j = 0; j < width; j++ ){
			X3[i][j] = X3[i][j] / max;								// X3=X3+abs(min(min(X3)))
			
			//copy new image (X3) back into X
			X[i*width + j] = (imbyte) (X3[i][j] * 255);
		}
	}
	
	imProcessCrop(imagetmp, dest, 0, 0);
	
	for ( i = 0; i < height; i++ ){
		free( matk[i] );
		free( matl[i] );
		free( X2[i] );
		free( X3[i] );
	}
	free( matk );
	free( matl );
	free( X2 );
	free( X3 );
	free( vk );
	free( vl );
}

/*
	ecrit les valeurs d'un tableaux d'int (fonction de debbuging)
*/
void imSaveValues( int *values, int count, const char *filename )
{
	FILE *fid = fopen(filename, "w" );
	if ( !fid )
		return;

	for(int i = 0; i < count; i++)
		fprintf(fid,"%d\t%d\n", i, values[i]);

	//fprintf(fid, "\n");
	fclose( fid );

}

/*
void SupOldFile::DistByCorrelationFFT(const _imImage *im1, const _imImage *im2,
                                wxSize window, int *decalageX, int *decalageY)
{
    wxASSERT_MSG(decalageX, wxT("decalageX cannot be NULL") );
    wxASSERT_MSG(decalageY, wxT("decalagY cannot be NULL") );
    wxASSERT_MSG(im1, wxT("Image 1 cannot be NULL") );
    wxASSERT_MSG(im2, wxT("Image 2 cannot be NULL") );

    imImage *corr = imImageCreate( im1->width, im1->height, im1->color_space, IM_CFLOAT);
    imProcessCrossCorrelation( im1, im2, corr );
    imImage *corrCrop = imImageCreate( window.GetWidth() * 2 + 1, window.GetHeight() * 2 + 1,
        corr->color_space, IM_CFLOAT );
    int xmin = im1->width / 2 - window.GetWidth();
    int ymin = im1->height / 2 - window.GetHeight();
    imProcessCrop( corr, corrCrop, xmin, ymin );

    imImage *corrReal = imImageCreate( corrCrop->width , corrCrop->height , corrCrop->color_space, IM_BYTE );
    imConvertDataType( corrCrop, corrReal, IM_CPX_MAG, IM_GAMMA_LINEAR, 0, IM_CAST_MINMAX);

    int width = corrReal->width;
    int height = corrReal->height;
    int max = 0, maxX = 0, maxY = 0;
    imbyte *buf = (imbyte*)corrReal->data[0];

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if ( buf[y * width + x] > max )
            {
                max = buf[y * width + x];
                maxX = x;
                maxY = y;

            }
        }
    }

    *decalageX = maxX - window.GetWidth();
    *decalageY = maxY - window.GetHeight();

    //int error;
    //imFile* ifile = NULL;
    //ifile = imFileNew("D:/Mes Images/corr1.tif", "TIFF", &error);
    //imFileSaveImage(ifile,corrReal);
    //imFileClose(ifile);

    imImageDestroy( corrReal );
    imImageDestroy( corrCrop );
    imImageDestroy( corr );
}
*/

/*
void DistByCorrelation( imImage *im1, imImage *im2, int width, int height, int *decalageX, int *decalageY)
{
    wxASSERT_MSG(decalageX, wxT("decalageX cannot be NULL") );
    wxASSERT_MSG(decalageY, wxT("decalagY cannot be NULL") );
    wxASSERT_MSG(im1, wxT("Image 1 cannot be NULL") );
    wxASSERT_MSG(im2, wxT("Image 2 cannot be NULL") );

    
	imProcessNegative( im1, im1 );
	imProcessNegative( im2, im2 );

	imImage *imTmp1 = imImageCreate(
            im1->width +  width * 2,
            im1->height +  height * 2,
            im1->color_space, im1->data_type);
	imProcessAddMargins( im1 ,imTmp1, width, height );


    int conv_width = 2 * width;
    int conv_height = 2 * height;
    imImage *mask = imImageCreate(im2->width, im2->height,im2->color_space, im2->data_type);
    imbyte *bufIm2 = (imbyte*)im2->data[0];
	
    int maxSum = 0, maxX = 0, maxY = 0;
    for (int y = 0; y < conv_height; y++)
    {
        for (int x = 0; x < conv_width; x++)
        {
            imProcessCrop(imTmp1,mask, x, y);
            imbyte *bufMask = (imbyte*)mask->data[0]; 
            int sum = 0;
            for (int i = 0; i < mask->plane_size; i++)
            {
                sum += (bufIm2[i] / 255) * (bufMask[i] / 255);
            }
            if (sum > maxSum)
            {
                maxSum = sum;
                maxX = x;
                maxY = y;
            }
        }
    }

    *decalageX = maxX - width;
    *decalageY = maxY - height;
    imImageDestroy(imTmp1);
    imImageDestroy(mask);
}
*/
