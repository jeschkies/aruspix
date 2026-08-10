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
    m_opImMapPalette.fill(0);
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

    int error;

    imFile* ifile = imFileOpen( file.c_str(), &error );
    if ( !ifile )
        return this->Terminate( ERR_FILE , (const char*)file.c_str() );

    // Load bitmap through IM; then copy into a cv::Mat, normalising to
    // single-channel 8-bit. Multi-channel (IM_RGB) sources are converted
    // via imConvertColorSpace before the copy so the numeric behaviour
    // matches the pre-swap pipeline.
    _imImage *loaded = imFileLoadBitmap( ifile, index, &error );
    imFileClose(ifile);
    if ( !loaded )
        return this->Terminate( ERR_READING , index, (const char*)file.c_str() );

    // Force IM_BYTE.
    if ( loaded->data_type != IM_BYTE ) {
        _imImage *tmp = imImageCreate( loaded->width, loaded->height, loaded->color_space, IM_BYTE );
        if (!tmp) { imImageDestroy(loaded); return this->Terminate( ERR_MEMORY ); }
        imConvertDataType( loaded, tmp, 0, 0, 0, 0 );
        imImageDestroy(loaded);
        loaded = tmp;
    }

    ImageDestroy(image);
    // Normalise RGB → grayscale at load time so downstream code always
    // sees single-channel 8-bit buffers. Use IM's own imConvertColorSpace
    // so the numeric behaviour matches the pre-swap pipeline.
    if ( imColorModeMatch(loaded->color_space, IM_RGB) ) {
        _imImage *gray = imImageCreate(loaded->width, loaded->height, IM_GRAY, IM_BYTE);
        if (!gray) { imImageDestroy(loaded); return this->Terminate( ERR_MEMORY ); }
        imConvertColorSpace(loaded, gray);
        imImageDestroy(loaded);
        loaded = gray;
    }

    image.create(loaded->height, loaded->width, CV_8UC1);
    std::memcpy(image.data, loaded->data[0], (size_t)loaded->height * loaded->width);
    if ( imColorModeMatch(loaded->color_space, IM_MAP) ) {
        // preserve palette metadata on the operator so WriteAsMAP can restore it later
        long *pal = loaded->palette;
        if (pal) {
            for (int i = 0; i < loaded->palette_count && i < 256; ++i)
                m_opImMapPalette[i] = pal[i];
        }
    }
    imImageDestroy(loaded);
    return true;
}

bool ImOperator::ExtractPlane( cv::Mat &image, cv::Mat &extracted_plane, int plane_number  )
{
	if ( !ConvertToMAP( image ) )
		return false;

    // Bridge to IM only for the bit-plane extract/reset on the IM_MAP-encoded
    // classification bitmask; the surrounding bitwise/arithmetic ops go
    // through cv:: directly.
    cv::Mat main_plane = image.clone();

    {
        ImView view_image(image, IM_MAP, m_opImMapPalette.data(), 256);
        ImView view_main(main_plane, IM_MAP, m_opImMapPalette.data(), 256);
        imProcessBitPlane( view_image, view_main, 0, 0 );
        imProcessBitPlane( view_image, view_image, 0, 1 ); // reset
    }
    cv::bitwise_not( main_plane, main_plane );
    cv::bitwise_or( main_plane, extracted_plane, main_plane );
    cv::bitwise_not( main_plane, main_plane );
    cv::add( image, main_plane, image );

    for (int i = 0; i < plane_number; i++ )
    {
        cv::add( extracted_plane, extracted_plane, extracted_plane );
    }
    {
        ImView view_image(image, IM_MAP, m_opImMapPalette.data(), 256);
        imProcessBitPlane( view_image, view_image, plane_number, 1 ); // reset
    }
    cv::add( image, extracted_plane, image );

	return true;
}


bool ImOperator::ConvertToMAP( cv::Mat &image )
{
    // In the new storage model, the image is always single-plane 8-bit.
    // ConvertToMAP historically wrapped a binary image (values 0/1) as MAP.
    // Nothing to do to the buffer — just refresh the palette metadata for
    // downstream WriteAsMAP().
    long *pal = imPaletteGray();
    pal[0] = imColorEncode( 255, 255, 255 ); // fond blanc
    pal[1] = imColorEncode( 0, 0, 0 ); // noir
    pal[2] = imColorEncode( 228, 228, 228 ); // gris = bord
    pal[4] = imColorEncode( 0, 127, 0 ); // vert fonce = lettrine
    pal[8] = imColorEncode( 0, 255, 0 ); // vert clair = texte dans portee
    pal[16] = imColorEncode( 255, 127, 0 ); // orange = texte
    pal[32] = imColorEncode( 255, 227, 0 ); // jaune = titre
    pal[64] = imColorEncode( 0, 0, 255 ); // bleu =
    pal[128] = imColorEncode( 255, 0, 255 ); // magenta =
    for (int i = 0; i < 256; ++i) m_opImMapPalette[i] = pal[i];
    (void)image;
	return true;
}


bool ImOperator::WriteAsMAP( wxString file, cv::Mat &image )
{
	wxASSERT_MSG( !file.IsEmpty(), "Filename  cannot be empty" );

	if ( !ConvertToMAP( image ) )
		return false;

    ImView view(image, IM_MAP, m_opImMapPalette.data(), 256);

    int error;
    imFile *ifile = imFileNew( file.c_str(), "TIFF", &error);
    if (error == IM_ERR_NONE)
    {
        imFileSetInfo( ifile, "RLE" );
        imImageSetAttribute( view, "Software", IM_BYTE, 8, "Aruspix" );
        imImageSetAttribute( view, "Author", IM_BYTE, 14, "Laurent Pugin" );
        wxLogNull logNo;
        error = imFileSaveImage( ifile, view );
        imFileClose(ifile);
    }

    if (error == IM_ERR_NONE)
        return true;
    else
        return this->Terminate( ERR_WRITING , (const char*)file.c_str());
}


bool ImOperator::Write( wxString file, const cv::Mat &image )
{
	wxASSERT_MSG( !file.IsEmpty(), "Filename  cannot be empty" );

    int error;
    int channels = image.channels();
    int color_space = (channels == 3) ? IM_RGB : IM_GRAY;
    ImView view(image, color_space);

    imFile *ifile = imFileNew( file.c_str(), "TIFF", &error);
    if (error == IM_ERR_NONE)
    {
        imFileSetInfo( ifile, "RLE" );
        imImageSetAttribute( view, "Software", IM_BYTE, 8, "Aruspix" );
        imImageSetAttribute( view, "Author", IM_BYTE, 14, "Laurent Pugin" );
        imImageSetAttribute( view, "Photometric", IM_BYTE, 1, "1");
        wxLogNull *logNo = new wxLogNull();
        error = imFileSaveImage( ifile, view );
		imFileClose(ifile);
        delete logNo;
        if (error == IM_ERR_NONE)
            return true;
        else
            return this->Terminate( ERR_WRITING , (const char*)file.c_str());
    }
    else
        return this->Terminate( ERR_WRITING , (const char*)file.c_str());
}

bool ImOperator::GetImagePlane( cv::Mat &image , int plane, int factor )
{
    if ( m_opImMap.empty() )
        return this->Terminate( ERR_UNKNOWN );

    ImageDestroy(image);
    // Extract bit-plane through IM to preserve exact semantics.
    image.create(m_opImMap.rows, m_opImMap.cols, CV_8UC1);
    {
        ImView src_view(m_opImMap, IM_MAP, m_opImMapPalette.data(), 256);
        ImView dst_view(image, IM_BINARY);
        imProcessBitPlane( src_view, dst_view, plane, 0 );
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

    return true;
}


bool ImOperator::GetImage( cv::Mat &image, int factor,  int binary_method, bool median_filtering )
{
    int color_type = IM_GRAY;
    if ( binary_method != -1 )
        color_type = IM_BINARY;

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

    // this prevent imProccessCrop to crash when the image is too small
    if ( (im2.cols < 4) || (im2.rows < 4) ) {
        return;
    }

    window.SetWidth( min( window.GetWidth(), im2.cols / 2  - 1) );
    window.SetHeight( min( window.GetHeight(), im2.rows / 2 - 1) );

    ImView view_im1(im1, IM_GRAY);
    ImView view_im2(im2, IM_GRAY);

    _imImage *corr = imImageCreate( im1.cols, im1.rows, IM_GRAY, IM_CFLOAT);
    imProcessCrossCorrelation( view_im1, view_im2, corr );

    _imImage *corrCrop = imImageCreate( window.GetWidth() * 2 + 1, window.GetHeight() * 2 + 1,
        IM_GRAY, IM_CFLOAT );
    int xmin = im1.cols / 2 - window.GetWidth();
    int ymin = im1.rows / 2 - window.GetHeight();
    imProcessCrop( corr, corrCrop, xmin, ymin );

    _imImage *corrReal = imImageCreate( corrCrop->width , corrCrop->height , corrCrop->color_space, IM_BYTE );
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

    if (maxCorr) *maxCorr = max;

    imImageDestroy( corrReal );
    imImageDestroy( corrCrop );
    imImageDestroy( corr );
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
