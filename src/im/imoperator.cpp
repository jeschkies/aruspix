/////////////////////////////////////////////////////////////////////////////
// Name:        imoperator.cpp
// Author:      Laurent Pugin
// Created:     04/05/25
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include <algorithm>
using std::min;
using std::max;

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#include <cstring>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "imoperator.h"
#include "analyze.h"
#include "image_ops.h"
#include "thresholds.h"

int ImOperator::s_pre_image_binarization_method = IM_BINARIZATION_OTSU;


//----------------------------------------------------------------------------
// ImOperator
//----------------------------------------------------------------------------


ImOperator::ImOperator( )
{
    m_progressDlg = NULL;
    m_error = ERR_NONE;

    m_opHist = NULL;
    m_opLines1 = NULL;
    m_opLines2 = NULL;
    m_opCols1 = NULL;

	m_pre_image_binarization_methodPtr = &ImOperator::s_pre_image_binarization_method;
}

ImOperator::~ImOperator()
{
}

bool ImOperator::Terminate( int code, ... )
{
    ImageDestroy(m_opImAlign);
    ImageDestroy(m_opImMask);
    ImageDestroy(m_opImTmp2);
    ImageDestroy(m_opImTmp1);
    ImageDestroy(m_opImMain);
    ImageDestroy(m_opIm);
    ImageDestroy(m_opImMap);

    if ( m_opHist )
        delete[] m_opHist;
    if ( m_opLines1 )
        delete[] m_opLines1;
    if ( m_opLines2 )
        delete[] m_opLines2;
    if ( m_opCols1 )
        delete[] m_opCols1;

    m_opHist = NULL;
    m_opLines1 = NULL;
    m_opLines2 = NULL;
    m_opCols1 = NULL;

    m_error = code;

    if ( code == ERR_NONE )
        return true; // normal ending


    // operation canceled
    if ( code == ERR_CANCELED )
    {
        wxLogMessage( _("Operation canceled") );
        return false;
    }

    // error
    va_list argptr;
    va_start( argptr, code );
    wxString msg;

    if ( code == ERR_UNKNOWN )
        msg = _("Unknown error");
    else if ( code == ERR_MEMORY )
        msg = _("Insufficient memory");
    else if ( code == ERR_FILE )
        msg.PrintfV( _("Error opening file '%s'"), argptr );
    else if ( code == ERR_READING )
        msg.PrintfV( _("Error reading image %d in file '%s'") , argptr );
    else if ( code == ERR_WRITING )
        msg.PrintfV( _("Error writing image in file '%s'") , argptr );

    va_end(argptr);
    wxLogError( msg );
    return false;
}

void ImOperator::SetProgressDlg( AxProgressDlg *dlg )
{
    wxASSERT_MSG( dlg, "Progress dialog pointer cannot be NULL");
    m_progressDlg = dlg;
}

void ImOperator::SetMapImage( const cv::Mat &image )
{
    ImageDestroy(m_opImMap);
    m_opImMap = image.clone();
}

bool ImOperator::Read( wxString file, cv::Mat &image, int index )
{
	wxASSERT_MSG( !file.IsEmpty(), "Filename  cannot be empty" );
    (void)index; // legacy multi-image TIFF index; cv::imread reads first only

    cv::Mat loaded = cv::imread( (const char*)file.c_str(), cv::IMREAD_UNCHANGED );
    if ( loaded.empty() )
        return this->Terminate( ERR_FILE, (const char*)file.c_str() );

    // Force 8-bit depth.
    if ( loaded.depth() != CV_8U ) {
        cv::Mat tmp;
        loaded.convertTo(tmp, CV_8U);
        loaded = tmp;
    }

    ImageDestroy(image);
    // Normalise multi-channel input (BGR / BGRA) to grayscale so downstream
    // code always sees single-channel 8-bit buffers. cv::cvtColor uses
    // Rec.601 weights, matching IM's imConvertColorSpace conversion.
    if ( loaded.channels() >= 3 ) {
        cv::cvtColor(loaded, image, loaded.channels() == 4 ? cv::COLOR_BGRA2GRAY
                                                            : cv::COLOR_BGR2GRAY);
    } else {
        image = loaded;
    }
    return true;
}

bool ImOperator::ExtractPlane( cv::Mat &image, cv::Mat &extracted_plane, int plane_number  )
{
	if ( !ConvertToMAP( image ) )
		return false;

    cv::Mat main_plane;
    ax::bit_plane_extract( image, main_plane, 0 );
    ax::bit_plane_reset( image, 0 );
    cv::bitwise_not( main_plane, main_plane );
    cv::bitwise_or( main_plane, extracted_plane, main_plane );
    cv::bitwise_not( main_plane, main_plane );
    cv::add( image, main_plane, image );

    for (int i = 0; i < plane_number; i++ )
    {
        cv::add( extracted_plane, extracted_plane, extracted_plane );
    }
    ax::bit_plane_reset( image, plane_number );
    cv::add( image, extracted_plane, image );

	return true;
}


bool ImOperator::ConvertToMAP( cv::Mat &image )
{
    // Legacy no-op: historically populated m_opImMapPalette with the
    // aruspix classification palette so WriteAsMAP could emit a
    // palette-indexed TIFF. WriteAsMAP now writes plain 8-bit gray, so
    // there's nothing to do here. Kept as a stub so existing callers
    // (ImPage::ExtractPlane, WriteAsMAP itself) don't need to change.
    (void)image;
	return true;
}


bool ImOperator::WriteAsMAP( wxString file, cv::Mat &image )
{
    // Previously wrote a palette-indexed IM_MAP TIFF so external viewers
    // could render classification planes in colour (green=ornate letter
    // etc.). The palette carried no functional information — Read() only
    // consumes the raw 8-bit bitmask. Drop the palette; write plain gray.
    return this->Write( file, image );
}


bool ImOperator::Write( wxString file, const cv::Mat &image )
{
	wxASSERT_MSG( !file.IsEmpty(), "Filename  cannot be empty" );

    // cv::imwrite chooses the codec by extension. TIFF is compressed via
    // libtiff's default (LZW); the pre-swap RLE ("packbits") was chosen for
    // historical compatibility with IM and offers no meaningful advantage.
    if ( !cv::imwrite( (const char*)file.c_str(), image ) )
        return this->Terminate( ERR_WRITING, (const char*)file.c_str() );
    return true;
}

bool ImOperator::GetImagePlane( cv::Mat &image , int plane, int factor )
{
    if ( m_opImMap.empty() )
        return this->Terminate( ERR_UNKNOWN );

    ImageDestroy(image);
    ax::bit_plane_extract( m_opImMap, image, plane );

    // resize
    for(int i = 1; i < factor; i*= 2 )
    {
        int removeX = image.cols % 2;
        int removeY = image.rows % 2;
        if (removeX || removeY) {
            image = image(cv::Rect(0, 0, image.cols - removeX, image.rows - removeY)).clone();
        }

        cv::Mat imTmp;
        cv::resize(image, imTmp, cv::Size(image.cols / 2, image.rows / 2), 0, 0, cv::INTER_AREA);
        image = imTmp;
    }

    return true;
}


bool ImOperator::GetImage( cv::Mat &image, int factor,  int binary_method, bool median_filtering )
{
    if ( m_opImMap.empty() )
        return this->Terminate( ERR_UNKNOWN );

    ImageDestroy(image);
    image = m_opImMap.clone();

    // binary
    if ( binary_method != -1 )
    {
        cv::Mat imTmp;
        if ( binary_method == IM_BINARIZATION_OTSU ) {
            cv::threshold(image, imTmp, 0, 1, cv::THRESH_BINARY | cv::THRESH_OTSU);
        } else if ( binary_method == IM_BINARIZATION_MINMAX ) {
            // imProcessMinMaxThreshold: T = (min + max) / 2
            double mn, mx;
            cv::minMaxLoc(image, &mn, &mx);
            cv::threshold(image, imTmp, (mn + mx) / 2.0, 1, cv::THRESH_BINARY);
        } else if ( binary_method == IM_BINARIZATION_BRINK ) {
            ax::brink2_classes_threshold(image, imTmp, false, BRINK_AND_PENDOCK);
        } else if ( binary_method == IM_BINARIZATION_BRINK3CLASSES ) {
            ax::brink3_classes_threshold(image, imTmp, false, BRINK_AND_PENDOCK);
        } else {
            wxLogWarning("Fix threshold used when resizing" );
            cv::threshold(image, imTmp, 127, 1, cv::THRESH_BINARY);
        }
        image = imTmp;
    }

    // resize
    for(int i = 1; i < factor; i*= 2 )
    {
        int removeX = image.cols % 2;
        int removeY = image.rows % 2;
        if (removeX || removeY) {
            image = image(cv::Rect(0, 0, image.cols - removeX, image.rows - removeY)).clone();
        }

        cv::Mat imTmp;
        cv::resize(image, imTmp, cv::Size(image.cols / 2, image.rows / 2), 0, 0, cv::INTER_AREA);
        image = imTmp;
    }

    // median filtering
    if ( median_filtering )
    {
        cv::Mat imTmp;
        cv::medianBlur(image, imTmp, 3);
        image = imTmp;
    }

    return true;
}


void ImOperator::PruneElementsZone( cv::Mat &image, int min_threshold, int max_threshold, int type )
{
    if (image.empty()) return;

    cv::Mat labels;
    int num_labels = cv::connectedComponents(image, labels, /*connectivity=*/4, CV_16U);
    int region_count = num_labels - 1;
    if (region_count <= 0)
        return;

    if      (type == IM_PRUNE_CLEAR_HEIGHT) ax::clear_height(labels, region_count, min_threshold, max_threshold);
    else if (type == IM_PRUNE_CLEAR_WIDTH)  ax::clear_width (labels, region_count, min_threshold, max_threshold);
    else /*    IM_PRUNE_CLEAR_MIN */        ax::clear_min   (labels, region_count, min_threshold);

    // Rewrite the byte plane: any surviving label -> 1, else 0.
    for (int y = 0; y < image.rows; ++y) {
        const uint16_t* r = labels.ptr<uint16_t>(y);
        uchar*          d = image.ptr<uchar>(y);
        for (int x = 0; x < image.cols; ++x)
            d[x] = r[x] ? 1 : 0;
    }
}


void ImOperator::MoveElements( cv::Mat &src, cv::Mat &dest, int boxes[], int count, int margins[4], int factor )
{
    for (int i = 0; i < count * 4; i += 4)
    {
        if ( (boxes[i+1] <= boxes[i+0]) || (boxes[i+3] <= boxes[i+2]) )
            continue;

        // bounding box
        cv::Mat box(
            (boxes[i+3] - boxes[i+2]) * factor,
            (boxes[i+1] - boxes[i+0]) * factor,
            CV_8UC1);
        box.setTo(255); // white (setTo(0) + bitwise_not collapses to setTo(255))

        int mx1 = max( factor * boxes[i+0] - margins[0] , 0 );
        int mx2 = min( factor * boxes[i+1] + margins[1] , src.cols - 1 );
        int my1 = max( factor * boxes[i+2] - margins[2] , 0 );
        int my2 = min( factor * boxes[i+3] + margins[3] , src.rows - 1  );
        int mmx1 = factor * boxes[i+0] - mx1;
        int mmy1 = factor * boxes[i+2] - my1;

        cv::Mat box_m1 = src(cv::Rect(mx1, my1, mx2 - mx1, my2 - my1)).clone();
        box.copyTo( box_m1(cv::Rect(mmx1, mmy1, box.cols, box.rows)) );

        cv::Mat box_mm1;
        cv::copyMakeBorder(box_m1, box_mm1, 1, 1, 1, 1, cv::BORDER_CONSTANT, cv::Scalar(0));
        ax::remove_by_area( box_mm1, box_mm1, 4, box.rows * box.cols, 0 );
        box_m1 = box_mm1(cv::Rect(1, 1, box_m1.cols, box_m1.rows)).clone();
        src(cv::Rect(mx1 + mmx1, my1 + mmy1, box.cols, box.rows)).copyTo( box );
        box.copyTo( box_m1(cv::Rect(mmx1, mmy1, box.cols, box.rows)) );
        box_m1.copyTo( dest(cv::Rect(mx1, my1, box_m1.cols, box_m1.rows)) );

        cv::Mat box_m2 = src(cv::Rect(mx1, my1, mx2 - mx1, my2 - my1)).clone();
        cv::bitwise_not( box_m1, box_m1 );
        cv::bitwise_and( box_m1, box_m2, box_m2 );
        box_m2.copyTo( src(cv::Rect(mx1, my1, box_m2.cols, box_m2.rows)) );
    }
}


// FFT don't works on binary images !!!!!!!

void ImOperator::DistByCorrelation( const cv::Mat &im1,  const cv::Mat &im2,
                                imSize window, int *decalageX, int *decalageY, int *maxCorr)
{
    wxASSERT_MSG(decalageX, wxT("decalageX cannot be NULL") );
    wxASSERT_MSG(decalageY, wxT("decalagY cannot be NULL") );
    wxASSERT_MSG(!im1.empty(), wxT("Image 1 cannot be NULL") );
    wxASSERT_MSG(!im2.empty(), wxT("Image 2 cannot be NULL") );

    // Skip if the source is too small to hold the requested window.
    if ( (im2.cols < 4) || (im2.rows < 4) ) {
        return;
    }

    window.SetWidth( min( window.GetWidth(), im2.cols / 2  - 1) );
    window.SetHeight( min( window.GetHeight(), im2.rows / 2 - 1) );

    // FFT-based cross-correlation. Mirrors IM's imProcessCrossCorrelation:
    // F1 * conj(F2) via cv::mulSpectrums(..., conjB=true), inverse DFT with
    // DFT_SCALE for the 1/N normalisation, then quadrant swap so origin
    // (zero shift) sits at the image centre.
    cv::Mat f1, f2;
    im1.convertTo(f1, CV_32F);
    im2.convertTo(f2, CV_32F);

    cv::Mat F1, F2;
    cv::dft(f1, F1, cv::DFT_COMPLEX_OUTPUT);
    cv::dft(f2, F2, cv::DFT_COMPLEX_OUTPUT);

    cv::Mat cross_spec;
    cv::mulSpectrums(F1, F2, cross_spec, 0, /*conjB=*/true);

    cv::Mat corr;
    cv::idft(cross_spec, corr, cv::DFT_SCALE | cv::DFT_REAL_OUTPUT);

    // FFT-shift to bring the zero-shift origin from (0,0) to (w/2, h/2).
    const int hw = corr.cols / 2;
    const int hh = corr.rows / 2;
    cv::Mat corr_shift(corr.size(), corr.type());
    corr(cv::Rect(hw, hh, corr.cols - hw, corr.rows - hh))
        .copyTo(corr_shift(cv::Rect(0, 0, corr.cols - hw, corr.rows - hh)));
    corr(cv::Rect(0, hh, hw, corr.rows - hh))
        .copyTo(corr_shift(cv::Rect(corr.cols - hw, 0, hw, corr.rows - hh)));
    corr(cv::Rect(hw, 0, corr.cols - hw, hh))
        .copyTo(corr_shift(cv::Rect(0, corr.rows - hh, corr.cols - hw, hh)));
    corr(cv::Rect(0, 0, hw, hh))
        .copyTo(corr_shift(cv::Rect(corr.cols - hw, corr.rows - hh, hw, hh)));

    // Crop the window around the (now-centred) origin.
    const int cw = window.GetWidth()  * 2 + 1;
    const int ch = window.GetHeight() * 2 + 1;
    const int xmin = im1.cols / 2 - window.GetWidth();
    const int ymin = im1.rows / 2 - window.GetHeight();
    cv::Mat corr_crop = corr_shift(cv::Rect(xmin, ymin, cw, ch)).clone();

    // Magnitude of a real cross-correlation is |value|; scale to 0..255
    // (IM_CAST_MINMAX) so *maxCorr matches the pre-swap byte-scaled value.
    cv::Mat corr_abs = cv::abs(corr_crop);
    double mn, mx;
    cv::minMaxLoc(corr_abs, &mn, &mx);
    cv::Mat corr_byte;
    if (mx > mn) {
        corr_abs.convertTo(corr_byte, CV_8U,
                           255.0 / (mx - mn), -255.0 * mn / (mx - mn));
    } else {
        corr_byte = cv::Mat::zeros(corr_abs.size(), CV_8U);
    }

    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(corr_byte, nullptr, &maxVal, nullptr, &maxLoc);

    *decalageX = maxLoc.x - window.GetWidth();
    *decalageY = maxLoc.y - window.GetHeight();
    if (maxCorr) *maxCorr = (int)maxVal;
}


void ImOperator::MedianFilter( int values[], int size, int filter_size, int *avg_ptr )
{
    int i;
    int pos, current_size;
    int half_size = filter_size / 2;
    int *tmp = new int[size];
    int avg = 0;

    for ( i = 0; i < size; i++ )
    {
        pos = ( i - half_size < 0 ) ? 0 : i - half_size;
        current_size = ( pos + filter_size > size - 1) ? size - pos : filter_size;
        int *win = new int[ current_size ];
        memcpy( win, values + pos, sizeof(int)*current_size);
        tmp[i] = median( win, current_size );
        avg += tmp[i];
        delete[] win;
    }
    memcpy( values, tmp, sizeof(int) * size);
    delete[] tmp;

    avg = avg / size;
    if ( avg_ptr )
        *avg_ptr = avg;
}
