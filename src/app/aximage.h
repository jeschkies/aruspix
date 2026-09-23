/////////////////////////////////////////////////////////////////////////////
// Name:        aximage.h
// Author:      Laurent Pugin
// Created:     2004
// Copyright (c) Authors and others. All rights reserved.   
/////////////////////////////////////////////////////////////////////////////

#ifndef __aximage_H__
#define __aximage_H__

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/image.h"

#include <opencv2/core.hpp>



//----------------------------------------------------------------------------
// wxAxImage
//----------------------------------------------------------------------------

class AxImage: public wxImage
{
public:
    // constructors and destructors
    AxImage();
	AxImage(const wxImage img);
    AxImage(int width, int height);
	bool LoadFile(const wxString& name, long type = wxBITMAP_TYPE_ANY, int index = -1);
	AxImage ScaleInterpolate(int width, int height, int interpolation);
	wxBitmap GetAxBitmap(int width = -1, int height = -1);
	void ShrinkDataInt(unsigned char *pInBuff, int wWidth, int wHeight,
                   unsigned char *pOutBuff, int wNewWidth, int wNewHeight);

        
private:
    
public:
	static bool s_zoomInterpolation;
	static bool s_reduceBigImages;
	static int s_imageSizeToReduce;
	bool static s_checkIfNegative;


private:
        DECLARE_CLASS(AxImage)
	
};


// ---------------------------------------------------------------------------
// cv::Mat <-> AxImage bridging helpers. These replace the earlier
// GetImImage / SetImImage pair, which round-tripped pixel data through
// the IM library. The functions preserve the vertical-flip semantics of
// the old code (IM images are bottom-left origin, wxImage is top-left)
// so the surrounding pipeline behaves identically. See aximage.cpp.
// ---------------------------------------------------------------------------

// Copy AxImage's RGB pixel data into a cv::Mat CV_8UC3 in BGR order,
// applying the same vertical flip that GetImImage(IM_RGB) performed.
cv::Mat GetCvMat(const AxImage *img);

// Fill AxImage from a cv::Mat. Accepts CV_8UC1 (grayscale — broadcast to
// R=G=B) or CV_8UC3 (BGR — reordered to RGB). Applies the same vertical
// flip that SetImImage performed on its way out to wxImage. Handles
// Destroy+Create when the size doesn't match.
void SetCvMat(AxImage *img, const cv::Mat &mat);


#endif // __AX_CORE_IMAGE_H__
