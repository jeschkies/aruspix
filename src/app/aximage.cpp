/////////////////////////////////////////////////////////////////////////////
// Name:        aximage.cpp
// Author:      Laurent Pugin
// Created:     2004
// Copyright (c) Authors and others. All rights reserved.   
/////////////////////////////////////////////////////////////////////////////

#include <algorithm>
using std::min;
using std::max;

#include <opencv2/imgproc.hpp>

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#include "aximage.h"

// statics
bool AxImage::s_reduceBigImages = true;
bool AxImage::s_zoomInterpolation = true;
int AxImage::s_imageSizeToReduce = 3000;
bool AxImage::s_checkIfNegative = true;


static int *CreateCoeffInt(int nLen, int nNewLen, bool bShrink)
{
	int nSum = 0, nSum2;
	int *pRes = new int[2 * nLen];
	int *pCoeff = pRes;
	int nNorm = (bShrink) ? (nNewLen << 12) / nLen : 0x1000;
	//int nNorm = (bShrink) ? (nNewLen * 4096) / nLen : 0x1000;
	int	nDenom = (bShrink)? nLen : nNewLen;

	memset((void*)pRes,0,2 * nLen * sizeof(int));

	for(int i = 0; i < nLen; i++, pCoeff += 2)
	{
		nSum2 = nSum + nNewLen;
		if(nSum2 > nLen)
		{
			*pCoeff = ((nLen - nSum) << 12) / nDenom;
			//*pCoeff = ((nLen - nSum) * 4096) / nDenom;
			pCoeff[1] = ((nSum2 - nLen) << 12) / nDenom;
			//pCoeff[1] = ((nSum2 - nLen) * 4096) / nDenom;
			nSum2 -= nLen;
		}
		else
		{
			*pCoeff = nNorm;
			if(nSum2 == nLen)
			{
				pCoeff[1] = -1;
				nSum2 = 0;
			}
		}
		nSum = nSum2;
	}
	return pRes;
}


//----------------------------------------------------------------------------
// AxImage
//----------------------------------------------------------------------------

IMPLEMENT_CLASS(AxImage, wxImage)

AxImage::AxImage(const wxImage img) 
						   : wxImage(img)
{

}

AxImage::AxImage(int width, int height) 
						   : wxImage(width,height)
{

}

AxImage::AxImage() : wxImage()
{
}


bool AxImage::LoadFile(const wxString& name, long type, int index)
{
	bool res = wxImage::LoadFile( name, (wxBitmapType)type, index);

	if (AxImage::s_reduceBigImages
		&& (max(this->GetWidth(),this->GetHeight()) > AxImage::s_imageSizeToReduce))
	{
		// Downsize by repeated 2x2-area reduction, matching the old
		// imProcessCrop (drop the last odd row/col) + imProcessReduceBy4
		// loop. cv::resize with INTER_AREA is the OpenCV equivalent of
		// ReduceBy4's box-filter downsample.
		cv::Mat img = GetCvMat(this);
		do
		{
			int removeX = img.cols % 2;
			int removeY = img.rows % 2;
			if (removeX || removeY) // odd dimensions: crop the last row/col
			{
				img = img(cv::Rect(0, 0, img.cols - removeX, img.rows - removeY)).clone();
			}
			cv::Mat next;
			cv::resize(img, next, cv::Size(img.cols / 2, img.rows / 2), 0, 0, cv::INTER_AREA);
			img = next;
		} while (max(img.cols, img.rows) > AxImage::s_imageSizeToReduce);
		SetCvMat(this, img);
	}
	//wxLogMessage("%d %d", this->GetWidth(), this->GetHeight());
	return res;
}



wxBitmap AxImage::GetAxBitmap(int width, int height)
{
	if ((width != -1) && (height != -1))
	{
		wxBitmap bmp(this->ScaleInterpolate(width,height,AxImage::s_zoomInterpolation));
		//wxBitmap bmp(this->Scale(width,height));
		return bmp;
	}
	else
	{
		//const wxImage img;
		//wxBitmap bmp( img );
		wxBitmap bmp( *this );
		return bmp;
	}
}

AxImage AxImage::ScaleInterpolate(int newWidth, int newHeight, int interpolation)
{
	AxImage scaledImg(newWidth, newHeight);
	int thisWidth = this->GetWidth();
	int thisHeight = this->GetHeight();
	double factor = (double)newWidth/(double)thisWidth;

	if (interpolation && (factor < 1.0))
	{
		ShrinkDataInt(this->GetData(),thisWidth,thisHeight,scaledImg.GetData(),newWidth,newHeight);
	}
	else
	{
		scaledImg = this->Scale(newWidth, newHeight);
	}
	return scaledImg;
}


void AxImage::ShrinkDataInt(unsigned char *pInBuff, int wWidth, int wHeight,
                   unsigned char *pOutBuff, int wNewWidth, int wNewHeight)
{
	// pdwBuff accumulates 32-bit sums per channel; we then pick the
	// most-significant byte to reconstruct an 8-bit RGB pixel. On
	// little-endian hosts that's byte index 3; on big-endian it's 0.
	int pixelToPickUp = 3;
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
	pixelToPickUp = 0;
#endif


	unsigned char  *pLine = pInBuff, *pPix;
	unsigned char  *pOutLine = pOutBuff;
	//int dwInLn = (3 * wWidth + 3) & ~3;
	//int dwOutLn = (3 * wNewWidth + 3) & ~3;
	int dwInLn = (3 * wWidth);
	int dwOutLn = (3 * wNewWidth);

	int   x, y, i, ii;
	bool  bCrossRow, bCrossCol;
	int   *pRowCoeff = CreateCoeffInt(wWidth, 
                                   wNewWidth,
                                   true);
	int   *pColCoeff = CreateCoeffInt(wHeight, 
                                   wNewHeight, 
                                   true);
	int *pXCoeff, *pYCoeff = pColCoeff;
	int dwBuffLn = 3 * wNewWidth * sizeof(int);
	int *pdwBuff = new int[9 * wNewWidth];
	int *pdwCurrLn = pdwBuff, 
       *pdwCurrPix, 
       *pdwNextLn = pdwBuff + 3 * wNewWidth;
	int dwTmp, *pdwNextPix;

	memset((void*)pdwBuff,0,3 * dwBuffLn);
	y = 0;
	while(y < wNewHeight - 1) 
		// normalement pas -1, mais sinon depasse le buffer (sauf si alloue par 
		// par windows !!!!! avec un bitmap) -> laisse une petite ligne noire en bas ... 
	{
		pPix = pLine;
		pLine += dwInLn;
		pdwCurrPix = pdwCurrLn;
		pdwNextPix = pdwNextLn;

		x = 0;
		pXCoeff = pRowCoeff;
		bCrossRow = pYCoeff[1] > 0;
		while(x < wNewWidth)
		{
			dwTmp = *pXCoeff * *pYCoeff;
			for(i = 0; i < 3; i++)
				pdwCurrPix[i] += dwTmp * pPix[i];
			bCrossCol = pXCoeff[1] > 0;
			if(bCrossCol)
			{
				dwTmp = pXCoeff[1] * *pYCoeff;
				for(i = 0, ii = 3; i < 3; i++, ii++)
					pdwCurrPix[ii] += dwTmp * pPix[i];
			}
			if(bCrossRow)
			{
				dwTmp = *pXCoeff * pYCoeff[1];
				for(i = 0; i < 3; i++)
					pdwNextPix[i] += dwTmp * pPix[i];
				if(bCrossCol)
				{
					dwTmp = pXCoeff[1] * pYCoeff[1];
					for(i = 0, ii = 3; i < 3; i++, ii++)
						pdwNextPix[ii] += dwTmp * pPix[i];
				}
			}
			if(pXCoeff[1])
			{
				x++;
				pdwCurrPix += 3;
				pdwNextPix += 3;
			}
			pXCoeff += 2;
			pPix += 3;
		}
		if(pYCoeff[1])
		{
			// set result line
			pdwCurrPix = pdwCurrLn;
			pPix = pOutLine;
			for(i = 3 * wNewWidth; i > 0; i--, pdwCurrPix++, pPix++)
				*pPix = ((unsigned char *)pdwCurrPix)[pixelToPickUp];
			// prepare line buffers
			pdwCurrPix = pdwNextLn;
			pdwNextLn = pdwCurrLn;
			pdwCurrLn = pdwCurrPix;
			memset(pdwNextLn, 0, dwBuffLn);

			y++;
			pOutLine += dwOutLn;
		}
		pYCoeff += 2;
	}
	delete [] pRowCoeff;
	delete [] pColCoeff;
	delete [] pdwBuff;
} 


/*AxImage AxImage::ScaleInterpolate(int width, int height, int interpolation)
{
	AxImage image;
	vigra::BImage *in = GetVigraBImage(this);
	try
	{

		if (in==NULL) return image;	
		
		vigra::BImage out(width, height);
                      
		switch(interpolation)
		{
			case 0:
				// resize the image, using a bi-cubic spline algorithms
				resizeImageNoInterpolation(srcImageRange(*in), 
					destImageRange(out));
				break;
			case 1:
				// resize the image, using a bi-cubic spline algorithms
				resizeImageLinearInterpolation(srcImageRange(*in), 
					destImageRange(out));
				break;
			default:
				// resize the image, using a bi-cubic spline algorithms
				resizeImageSplineInterpolation(srcImageRange(*in), 
					destImageRange(out));
		}
		delete in;
		SetVigraBImage(&out,&image);
		return image;

	}
	catch (vigra::StdException & e)
	{
		wxLogError(wxString::Format("%s",e.what()));
		delete in;
        return image;
	}

}*/


cv::Mat GetCvMat(const AxImage *img)
{
	if (!img || !img->IsOk()) return cv::Mat();

	// wxImage stores 3-byte interleaved R,G,B with top-left origin.
	// Wrap that buffer as a CV_8UC3 header, convert to BGR (OpenCV
	// convention), then vertically flip to match the old GetImImage
	// contract (IM_RGB image with bottom-left origin).
	cv::Mat rgb(img->GetHeight(), img->GetWidth(), CV_8UC3, img->GetData());
	cv::Mat bgr;
	cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
	cv::flip(bgr, bgr, 0);
	return bgr;
}

void SetCvMat(AxImage *img, const cv::Mat &mat)
{
	if (!img || mat.empty()) return;

	if (!img->IsOk() || (mat.cols != img->GetWidth()) || (mat.rows != img->GetHeight()))
	{
		img->Destroy();
		img->Create(mat.cols, mat.rows);
	}

	// Vertical flip to reverse the bottom-left origin used by the pre-cv
	// pipeline (matches the imProcessFlip that SetImImage performed).
	cv::Mat flipped;
	cv::flip(mat, flipped, 0);

	cv::Mat rgb;
	if (flipped.type() == CV_8UC1)
	{
		// Broadcast grayscale to R=G=B for wxImage.
		cv::cvtColor(flipped, rgb, cv::COLOR_GRAY2RGB);
	}
	else if (flipped.type() == CV_8UC3)
	{
		// Assume BGR (OpenCV convention) — swap to RGB for wxImage.
		cv::cvtColor(flipped, rgb, cv::COLOR_BGR2RGB);
	}
	else
	{
		wxFAIL_MSG("SetCvMat: expected CV_8UC1 or CV_8UC3");
		return;
	}

	memcpy(img->GetData(), rgb.data,
	       (size_t)rgb.cols * rgb.rows * 3);
}





