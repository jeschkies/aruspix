/*
 *  imbrink3classes.cpp
 *
 *
 *  Created by Tristan Himmelman on 5/8/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "thresholds.h"

#include <cmath>
#include <cstdlib>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#define MAX_GREY 256

namespace {
void calc_gray_hist_256(const cv::Mat &src, unsigned long out[MAX_GREY]) {
    int histSize = 256;
    float range[] = {0.0f, 256.0f};
    const float *histRange = range;
    cv::Mat histMat;
    cv::calcHist(&src, 1, nullptr, cv::Mat(), histMat, 1,
                 &histSize, &histRange);
    for (int i = 0; i < MAX_GREY; ++i)
        out[i] = static_cast<unsigned long>(histMat.at<float>(i));
}
}  // namespace

static double sum256( const double *array, int size){
	double sum = 0;
	for ( int i = 0;  i < size; i++ )
		sum += array[i];

	return sum;
}

static void cumSum( const double *array, int size, double *dest ){
	if ( size < 1 )
		exit( EXIT_FAILURE );

	dest[0] = array[0];
	for ( int i = 1; i < size; i++ )
		dest[i] = array[i] + dest[i-1];
}

namespace ax {

int brink2_classes_threshold(const cv::Mat& src_in, cv::Mat& dst,
                             bool white_is_255, int algorithm)
{
	if (src_in.type() != CV_8UC1) return 0;
	cv::Mat src = src_in.clone();
	if (!white_is_255) src = 255 - src;

	int i;
	unsigned long imhist[MAX_GREY];
	double h[MAX_GREY];
	double count_f[MAX_GREY], count_b[MAX_GREY];
	double mu_f[MAX_GREY], mu_b[MAX_GREY];
	double KL_f_base[MAX_GREY], KL_b_base[MAX_GREY];
	double KL_f[MAX_GREY], KL_b[MAX_GREY];

	calc_gray_hist_256(src, imhist);
	for ( i = 0; i< MAX_GREY; i++) h[i] = imhist[i];

	double N = sum256(h, MAX_GREY);

	/* Counts for each threshold for each pixel class */
	cumSum( h, MAX_GREY, count_f );

	for ( i = 0; i < MAX_GREY; i++ )
		count_b[i] = N - count_f[i];

	/* Moments for each threshold for each pixel class */
	for ( i = 0; i < MAX_GREY; i++ )
		mu_f[i] = ( h[i] * ( i + 1 ) );

	cumSum( mu_f, MAX_GREY, mu_f );						//mu_f = cumsum(h' .* g)

	for ( i = 0; i < MAX_GREY; i++ )
		mu_b[i] = mu_f[MAX_GREY - 1] - mu_f[i];			//mu_b = mu_f(end) - mu_f

	for ( i = 0; i < MAX_GREY; i++ ){
		if ( count_f[i] == 0 ) mu_f[i] = 0;
		else mu_f[i] = mu_f[i] / count_f[i];			//mu_f = mu_f ./ count_f;

		if ( count_b[i] == 0 ) mu_b[i] = 0;
		else mu_b[i] = mu_b[i] / count_b[i];			//mu_b = mu_b ./ count_b;
	}

	/* Threshold-dependent computations for KL divergences */
	for ( i = 0; i < MAX_GREY; i++ )
		KL_f_base[i] = h[i] * log(i+1);

	cumSum( KL_f_base, MAX_GREY, KL_f_base );					//KL_f_base = cumsum(h' .* log(g));

	for ( i = 0; i < MAX_GREY; i++ )
		KL_b_base[i] = KL_f_base[MAX_GREY - 1] - KL_f_base[i];	//KL_b_base = KL_f_base(end) - KL_f_base;

	if ( algorithm == BRINK_AND_PENDOCK ){
		double KL_f_base2[MAX_GREY], KL_b_base2[MAX_GREY];

		/* Another set of threshold-dependent computations */
		for ( i = 0; i < MAX_GREY; i++ )
			KL_f_base2[i] = h[i] * (i+1) * log(i+1);

		cumSum( KL_f_base2, MAX_GREY, KL_f_base2 );				//KL_f_base2 = cumsum(h' .* g .* log(g));

		for ( i = 0; i < MAX_GREY; i++ )
			KL_b_base2[i] = KL_f_base2[MAX_GREY-1] - KL_f_base2[i];	//KL_b_base2 = KL_f_base2(end) - KL_f_base2;

		/* Component KL divergences. Same format as for moments */
		for ( i = 0; i < MAX_GREY; i++ ){
			KL_f[i] = KL_f_base2[i] - ( mu_f[i] * KL_f_base[i] );	//KL_f = KL_f_base2 - mu_f .* KL_f_base;
			KL_b[i] = KL_b_base2[i] - ( mu_b[i] * KL_b_base[i] );	//KL_b = KL_b_base2 - mu_b .* KL_b_base;
		}
	} else if ( algorithm == LI_AND_LEE ){
		double mu_f_term[MAX_GREY], mu_b_term[MAX_GREY];

		/* Moment-dependent terms for KL divergences */
		for ( i = 0; i < MAX_GREY; i++ ){
			if ( mu_f[i] == 0.0 ) mu_f_term[i] = 0.0;		//Check to prevent taking log(0)
			else mu_f_term[i] = mu_f[i] * log( mu_f[i] );	//mu_f_term = mu_f .* log(mu_f);

			if ( mu_b[i] == 0.0 ) mu_b_term[i] = 0.0;		//Check to prevent taking log(0)
			else mu_b_term[i] = mu_b[i] * log( mu_b[i] );	//mu_b_term = mu_b .* log(mu_b);
		}

		/* Component KL divergences. Same format as for moments. */
		for ( i = 0; i < MAX_GREY; i++ ){
			KL_f[i] = ( count_f[i] * mu_f_term[i] ) - ( mu_f[i] * KL_f_base[i] );	//KL_f = count_f .* mu_f_term - mu_f .* KL_f_base
			KL_b[i] = ( count_b[i] * mu_b_term[i] ) - ( mu_b[i] * KL_b_base[i] );	//KL_b = count_b .* mu_b_term - mu_b .* KL_b_base;
		}
	}

	double  minVal = KL_f[0] + KL_b[0];
	int T = 0;
	for ( i = 0; i < MAX_GREY; i++ ){
		double tempVal = KL_f[i] + KL_b[i];

		if ( minVal > tempVal ){
			minVal = tempVal;		//Find minimum
			T = i;
		}
	}

	dst.create(src.rows, src.cols, CV_8UC1);
	cv::threshold(src, dst, T, 1, cv::THRESH_BINARY_INV);

	return T;
}

int brink3_classes_threshold(const cv::Mat& src_in, cv::Mat& dst,
                             bool white_is_255, int algorithm)
{
	if (src_in.type() != CV_8UC1) return 0;
	cv::Mat src = src_in.clone();
	if (!white_is_255) src = 255 - src;

	int G = 256;
	int i , j;
	unsigned long imhist[MAX_GREY];
	double h[MAX_GREY];
	double accum[MAX_GREY];

	// Heap-allocated 2D arrays (256x256 doubles = 512 KiB each × 12
	// exceeds typical stack sizes). std::vector<std::vector<double>>
	// replaces the former alloc2DArray/free2DArray helpers.
	auto make2d = [](int rows, int cols) {
		return std::vector<std::vector<double>>(rows,
		    std::vector<double>(cols, 0.0));
	};
	auto count_f = make2d(MAX_GREY, MAX_GREY);
	auto count_b = make2d(MAX_GREY, MAX_GREY);
	auto count_t = make2d(MAX_GREY, MAX_GREY);
	auto mu_f    = make2d(MAX_GREY, MAX_GREY);
	auto mu_b    = make2d(MAX_GREY, MAX_GREY);
	auto mu_t    = make2d(MAX_GREY, MAX_GREY);
	auto KL_f_base = make2d(MAX_GREY, MAX_GREY);
	auto KL_b_base = make2d(MAX_GREY, MAX_GREY);
	auto KL_t_base = make2d(MAX_GREY, MAX_GREY);
	auto KL_f    = make2d(MAX_GREY, MAX_GREY);
	auto KL_b    = make2d(MAX_GREY, MAX_GREY);
	auto KL_t    = make2d(MAX_GREY, MAX_GREY);

	calc_gray_hist_256(src, imhist);
	for ( i = 0; i < MAX_GREY; i++ )
		h[i] = imhist[i];

	double N = sum256( h, MAX_GREY );
	cumSum( h, MAX_GREY, accum );								//accum = cumsum(h');

	for ( i = 0; i < G; i++ )
		for ( j = 0; j < MAX_GREY; j++ )
			count_f[i][j] = accum[j];							//count_f = repmat(accum, G, 1);

	for ( i = 0; i < G; i++ )
		for ( j = 0; j < MAX_GREY; j++ )
			count_b[j][i] = N - accum[j];						//count_b = N - repmat(accum', 1, G);

	for  ( i = 0; i < MAX_GREY; i++ )
		for ( j = 0; j < MAX_GREY; j++ )
			count_t[i][j] = N - count_f[i][j] - count_b[i][j];	//count_t = N - count_f - count_b;

	/* Moments for each pair of thresholds for each pixel class. Same format as the counts. */
	for ( i = 0; i < MAX_GREY; i++ )
		accum[i] = h[i] * (i + 1);

	cumSum( accum, MAX_GREY, accum );										//accum = cumsum(h' .* g);

	for ( i = 0; i < G; i++ )
		for ( j = 0; j < MAX_GREY; j++ )
			mu_f[i][j] = accum[j];											//mu_f = repmat(accum, G, 1);

	for ( i = 0; i < G; i++ )
		for ( j = 0; j < MAX_GREY; j++ )
			mu_b[j][i] = accum[MAX_GREY - 1] - accum[j];					//mu_b = repmat(accum(end) - accum', 1, G);

	for ( i = 0; i < MAX_GREY; i++ )
		for ( j = 0; j < MAX_GREY; j++ )
			mu_t[i][j] = accum[MAX_GREY - 1] - mu_f[i][j] - mu_b[i][j];		//mu_t = accum(end) - mu_f - mu_b;

	for ( i = 0; i < MAX_GREY; i++ ){
		for ( j = 0; j < MAX_GREY; j++ ){
			if ( count_f[i][j] == 0.0 ) mu_f[i][j] = 0.0;
			else mu_f[i][j] = mu_f[i][j] / count_f[i][j];					//mu_f = mu_f ./ count_f;

			if ( count_b[i][j] == 0.0 ) mu_b[i][j] = 0.0;
			else mu_b[i][j] = mu_b[i][j] / count_b[i][j];					//mu_b = mu_b ./ count_b;

			if ( count_t[i][j] == 0.0 ) mu_t[i][j] = 0.0;
			else mu_t[i][j] = mu_t[i][j] / count_t[i][j];					//mu_t = mu_t ./ count_t;
		}
	}

	/* Threshold-dependent computations for KL divergences. Same format as for moments. */
	for ( i = 0; i < MAX_GREY; i++ )
		accum[i] = h[i] * log(i+1);
	cumSum( accum, MAX_GREY, accum );													//accum = cumsum(h' .* log(g));

	for ( i = 0; i < G; i++ )
		for ( j = 0; j < MAX_GREY; j++ )
			KL_f_base[i][j] = accum[j];													//KL_f_base = repmat(accum, G, 1);

	for ( i = 0; i < G; i++ )
		for ( j = 0; j < MAX_GREY; j++ )
			KL_b_base[j][i] = accum[MAX_GREY-1] - accum[j];								//KL_b_base = repmat(accum(end) - accum', 1, G);

	for ( i = 0; i < MAX_GREY; i++ )
		for ( j = 0; j < MAX_GREY; j++ )
			KL_t_base[i][j] = accum[MAX_GREY-1] - KL_f_base[i][j] - KL_b_base[i][j];	//KL_t_base = accum(end) - KL_f_base - KL_b_base;

	/* Algorithm Selection */
	if ( algorithm == BRINK_AND_PENDOCK ){
		auto KL_f_base2 = make2d(MAX_GREY, MAX_GREY);
		auto KL_b_base2 = make2d(MAX_GREY, MAX_GREY);
		auto KL_t_base2 = make2d(MAX_GREY, MAX_GREY);

		/* Another set of threshold-dependent computations. */
		for ( i = 0; i < MAX_GREY; i++ )
			accum[i] = h[i] * (i+1) * log(i+1);
		cumSum( accum, MAX_GREY, accum );													//accum = cumsum(h' .* g .* log(g));

		for ( i = 0; i < G; i++ )
			for ( j = 0; j < MAX_GREY; j++ )
				KL_f_base2[i][j] = accum[j];												//KL_f_base2 = repmat(accum, G, 1);

		for ( i = 0; i < G; i++ )
			for ( j = 0; j < MAX_GREY; j++ )
				KL_b_base2[j][i] = accum[MAX_GREY-1] - accum[j];							//KL_b_base2 = repmat(accum(end) - accum', 1, G);

		for ( i = 0; i < MAX_GREY; i++ )
			for ( j = 0; j < MAX_GREY; j++ )
				KL_t_base2[i][j] = accum[MAX_GREY-1] - KL_f_base2[i][j] - KL_b_base2[i][j];	//KL_t_base2 = accum(end) - KL_f_base2 - KL_b_base2;

		/* Component KL divergences. Same format as for moments. */
		for ( i = 0; i < MAX_GREY; i++ ){
			for ( j = 0; j < MAX_GREY; j++ ){
				KL_f[i][j] = KL_f_base2[i][j] - ( mu_f[i][j] * KL_f_base[i][j] );			//KL_f = KL_f_base2 - mu_f .* KL_f_base;
				KL_b[i][j] = KL_b_base2[i][j] - ( mu_b[i][j] * KL_b_base[i][j] );			//KL_b = KL_b_base2 - mu_b .* KL_b_base;
				KL_t[i][j] = KL_t_base2[i][j] - ( mu_t[i][j] * KL_t_base[i][j] );			//KL_t = KL_t_base2 - mu_t .* KL_t_base;
			}
		}
	} else if ( algorithm == LI_AND_LEE ){
		auto mu_f_term = make2d(MAX_GREY, MAX_GREY);
		auto mu_b_term = make2d(MAX_GREY, MAX_GREY);
		auto mu_t_term = make2d(MAX_GREY, MAX_GREY);

		/* Moment-dependent terms for KL divergences. Same format as moments. */
		for ( i = 0; i < MAX_GREY; i++ ){
			for ( j = 0; j < MAX_GREY; j++ ){
				if ( mu_f[i][j] == 0.0 ) mu_f_term[i][j] = 0.0;					//mu_f_term = mu_f .* log(mu_f);
				else mu_f_term[i][j] = mu_f[i][j] * log( mu_f[i][j] );			//mu_f_term(isnan(mu_f_term)) = 0;

				if ( mu_b[i][j] == 0.0 ) mu_b_term[i][j] = 0.0;					//mu_b_term = mu_b .* log(mu_b);
				else mu_b_term[i][j] = mu_b[i][j] * log( mu_b[i][j] );	 		//mu_b_term(isnan(mu_b_term)) = 0;

				if ( mu_t[i][j] == 0.0 ) mu_t_term[i][j] = 0.0;					//mu_t_term = mu_t .* log(mu_t);
				else mu_t_term[i][j] = mu_t[i][j] * log( mu_t[i][j] );			//mu_t_term(isnan(mu_t_term)) = 0;
			}
		}

		/* Component KL divergences. Same format as for moments. */
		for ( i = 0; i < MAX_GREY; i++ ){
			for ( j = 0; j < MAX_GREY; j++ ){
				KL_f[i][j] = ( count_f[i][j] * mu_f_term[i][j] ) - ( mu_f[i][j] * KL_f_base[i][j] );	//KL_f = count_f .* mu_f_term - mu_f .* KL_f_base;
				KL_b[i][j] = ( count_b[i][j] * mu_b_term[i][j] ) - ( mu_b[i][j] * KL_b_base[i][j] );	//KL_b = count_b .* mu_b_term - mu_b .* KL_b_base;
				KL_t[i][j] = ( count_t[i][j] * mu_t_term[i][j] ) - ( mu_t[i][j] * KL_t_base[i][j] );	//KL_t = count_t .* mu_t_term - mu_t .* KL_t_base;
			}
		}
	}

	double KLmin[MAX_GREY];
	for ( i = 0; i < MAX_GREY; i++ ){
		double  minVal = KL_f[0][i] + KL_b[0][i] + KL_t[0][i];
		for ( j = 0; j < MAX_GREY; j++ ){
			double tempVal = KL_f[j][i] + KL_b[j][i] + KL_t[j][i];

			if ( minVal > tempVal ) minVal = tempVal;
		}
		KLmin[i] = minVal;
	}

	//Find index of minimum
	double minVal = KLmin[0];
	int T = 0;
	for ( i = 0; i < MAX_GREY; i++){
		if ( minVal > KLmin[i] ){
			minVal = KLmin[i];
			T = i;
		}
	}

	dst.create(src.rows, src.cols, CV_8UC1);
	cv::threshold(src, dst, T, 1, cv::THRESH_BINARY_INV);

	return T;
}

}  // namespace ax
