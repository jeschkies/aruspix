/////////////////////////////////////////////////////////////////////////////
// Name:        imstaffsegment.cpp
// Author:      Laurent Pugin
// Created:     2004
// Copyright (c) Authors and others. All rights reserved.   
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#include "imstaffsegment.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>


//----------------------------------------------------------------------------
// ImStaffSegment
//----------------------------------------------------------------------------

ImStaffSegment::ImStaffSegment(  ) :
    ImOperator( )
{

}

ImStaffSegment::~ImStaffSegment()
{
}

bool ImStaffSegment::AnalyzeSegment()
{
    wxASSERT_MSG( !m_opImMap.empty(), wxT("MAP Image cannot be NULL") );

    if ( !GetImagePlane( m_opImMain ) )
        return false;

    cv::Mat &src = m_opImMain;

    // 1-pixel background margin so components touching the edge don't
    // fuse with the border in the following morphological close.
    cv::Mat bordered;
    cv::copyMakeBorder(src, bordered, 1, 1, 1, 1,
                       cv::BORDER_CONSTANT, cv::Scalar(0));

    // 5x5 binary morphological close, single iteration.
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT,
                                                cv::Size(5, 5));
    cv::morphologyEx(bordered, bordered, cv::MORPH_CLOSE, kernel);

    // 8-connectivity connected components with per-label area stats.
    cv::Mat labels, stats, centroids;
    int region_count = cv::connectedComponentsWithStats(
        bordered, labels, stats, centroids, /*connectivity=*/8, CV_32S);

    // Sum compactness across foreground labels. The original per-region
    // term is:   perim^2 / (4*pi*area) * (area / width)
    // where `area / width` is INTEGER division on ints — small regions
    // (area < width) therefore contribute zero. Preserved so the
    // downstream ratio check in ImPage stays aligned.
    float c = 0;
    for (int label = 1; label < region_count; ++label) {
        int area = stats.at<int>(label, cv::CC_STAT_AREA);

        cv::Mat mask = (labels == label);
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL,
                         cv::CHAIN_APPROX_NONE);
        double perim = 0.0;
        for (const auto& contour : contours) {
            perim += cv::arcLength(contour, /*closed=*/true);
        }

        c += pow(perim, 2) / (4 * AX_PI * area) * (area / bordered.cols);
    }
    this->m_compactness = c;

    return this->Terminate( ERR_NONE );
}

