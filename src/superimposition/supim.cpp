/////////////////////////////////////////////////////////////////////////////
// Name:        supim.cpp
// Author:      Laurent Pugin
// Created:     2004
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifdef AX_SUPERIMPOSITION

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#include <algorithm>
using std::min;
using std::max;

#include "supim.h"
#include "sup.h"
#include "supfile.h"

#include "app/axapp.h"
//#include "app/axframe.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>



//----------------------------------------------------------------------------
// SupImSrcWindow
//----------------------------------------------------------------------------

IMPLEMENT_CLASS(SupImSrcWindow, AxScrolledWindow)


BEGIN_EVENT_TABLE(SupImSrcWindow,AxScrolledWindow)
END_EVENT_TABLE()

SupImSrcWindow::SupImSrcWindow( AxImageController *parent, wxWindowID id,
    const wxPoint &position, const wxSize& size, long style ) :
    AxScrolledWindow( parent, id, position, size, style )
{
    m_circleCenter = wxPoint(0,0);
}

SupImSrcWindow::SupImSrcWindow()
{
}

SupImSrcWindow::~SupImSrcWindow()
{
}

void SupImSrcWindow::SetCirclePen( const wxPen *pen, int width )
{
    m_pen = *pen;
    m_pen.SetWidth( width );
}

void SupImSrcWindow::DrawCircle( )
{
    wxASSERT_MSG(m_bitmap,"Bitmap cannot be NULL");
    wxASSERT_MSG(m_bufferBitmap,"Buffer bitmap cannot be NULL");

    // calculate scroll position
    int scrollX, scrollY;
    this->GetViewStart(&scrollX,&scrollY);
    int unitX, unitY;
    this->GetScrollPixelsPerUnit(&unitX,&unitY);
    scrollX *= unitX;
    scrollY *= unitY;

    wxMemoryDC memDC;
    memDC.SelectObject(*m_bufferBitmap);
    
    wxMemoryDC bufDC;
    bufDC.SelectObject(*m_bitmap);
    bufDC.SetBrush( *wxTRANSPARENT_BRUSH );
    bufDC.SetPen( m_pen );

    int clientX, clientY;
    this->GetClientSize( &clientX, &clientY );

    bufDC.Blit(scrollX, scrollY, clientX, clientY,
        &memDC, scrollX, scrollY );

    memDC.SelectObject( wxNullBitmap );
    
    int radius = 40; 
    bufDC.DrawCircle( m_circleCenter.x, m_circleCenter.y, radius);

    wxClientDC dc(this);
    dc.Blit(0, 0, clientX, clientY,
        &bufDC, scrollX, scrollY);

    bufDC.SelectObject( wxNullBitmap );
}

void SupImSrcWindow::ScrollSource( double x, double y )
{
    // calculate scroll position
    int scrollX = (int)(x * m_scale);
    int scrollY = (int)(y * m_scale);

    int clientX, clientY;
    this->GetClientSize( &clientX, &clientY );

    m_circleCenter.x = scrollX;
    m_circleCenter.y = scrollY;

    scrollX -= clientX / 2;
    scrollY -= clientY / 2;
    
    int unitX, unitY;
    this->GetScrollPixelsPerUnit( &unitX,&unitY );

    if ( unitX )
        scrollX /= unitX;
    if ( unitY )
        scrollY /= unitY;

    scrollX = ( scrollX < 0 ) ? 0 : scrollX;
    scrollY = ( scrollY < 0 ) ? 0 : scrollY;

    this->Scroll( scrollX, scrollY );
}



//----------------------------------------------------------------------------
// SupImWindow
//----------------------------------------------------------------------------

IMPLEMENT_CLASS(SupImWindow, AxScrolledWindow)


BEGIN_EVENT_TABLE(SupImWindow,AxScrolledWindow)
    EVT_PAINT( SupImWindow::OnPaint )
    EVT_MOUSE_EVENTS( SupImWindow::OnMouse )
END_EVENT_TABLE()

SupImWindow::SupImWindow( AxImageController *parent, wxWindowID id,
    const wxPoint &position, const wxSize& size, long style ) :
    AxScrolledWindow( parent, id, position, size, style )
{
}

SupImWindow::SupImWindow()
{
}

SupImWindow::~SupImWindow()
{
}

void SupImWindow::SynchronizeScroll( int x, int y )
{

    wxASSERT_MSG( m_imControlPtr,"Image controller cannot be NULL" );
    wxClassInfo *info;
    info = m_imControlPtr->GetClassInfo();
    wxASSERT_MSG( info,"Class info cannot be NULL" );
    wxASSERT_MSG( info->IsKindOf( CLASSINFO( SupImController ) ),
        "Controller must be a SupImController");

    SupImController *controller = (SupImController*)m_imControlPtr;

    // calculate scroll position
    int scrollX, scrollY;
    this->GetViewStart(&scrollX,&scrollY);
    int unitX, unitY;
    this->GetScrollPixelsPerUnit(&unitX,&unitY);
    scrollX *= unitX;
    scrollY *= unitY;

    double absX = (double)(x + scrollX) / m_scale;
    double absY = (double)(y + scrollY) / m_scale;
        
    controller->ScrollSources( absX, absY );
    this->SetFocus();
}

void SupImWindow::OnPaint(wxPaintEvent &event)
{
    if (m_bitmap && m_bitmap->IsOk() ) {
        wxMemoryDC dc;
        dc.SelectObject(*m_bitmap);
        
        // draw points
        SupImController *controller = (SupImController*)m_imControlPtr;
        
        if ( controller->m_supFilePtr->m_hasManualPoints1  && !controller->m_supFilePtr->IsSuperimposed() ) {
            
            wxPen pen;
            pen.SetWidth( max( ToZoomedRender( 4 ), 1 ) );
            imPoint *points;
            if ( controller->GetId() == ID2_CONTROLLER1) {
                points = controller->m_supFilePtr->m_points1;
                pen.SetColour( *wxGREEN );
            }
            else {
                points = controller->m_supFilePtr->m_points2; 
                pen.SetColour( *wxRED );
            }
            
            int radius = ToZoomedRender( 10 );
            dc.SetBrush( *wxTRANSPARENT_BRUSH );
            dc.SetPen( pen  );
            dc.DrawCircle( ToZoomedRender( controller->ToRender( points[0] ) ), radius );
            dc.DrawCircle( ToZoomedRender( controller->ToRender( points[1] ) ), radius );
            dc.DrawCircle( ToZoomedRender( controller->ToRender( points[2] ) ), radius );
            dc.DrawCircle( ToZoomedRender( controller->ToRender( points[3] ) ), radius );
            dc.DrawLine( ToZoomedRender( controller->ToRender( points[0] ) ), ToZoomedRender( controller->ToRender( points[1] ) ) );
            dc.DrawLine( ToZoomedRender( controller->ToRender( points[1] ) ), ToZoomedRender( controller->ToRender( points[3] ) ) );
            dc.DrawLine( ToZoomedRender( controller->ToRender( points[3] ) ), ToZoomedRender( controller->ToRender( points[2] ) ) );  
            dc.DrawLine( ToZoomedRender( controller->ToRender( points[2] ) ), ToZoomedRender( controller->ToRender( points[0] ) ) );
            dc.SetBackground( wxNullBrush );
            //dc.DrawPolygon( 4, controller->m_supFilePtr->m_points1 );
            
        }
    }
    event.Skip();
    
}


void SupImWindow::OnMouse(wxMouseEvent &event)
{

    wxASSERT_MSG( m_imControlPtr,"Image controller cannot be NULL" );
    wxClassInfo *info;
    info = m_imControlPtr->GetClassInfo();
    wxASSERT_MSG( info,"Class info cannot be NULL" );
    wxASSERT_MSG( info->IsKindOf( CLASSINFO( SupImController ) ),
        "Controller must be a SupImController");

    SupImController *controller = (SupImController*)m_imControlPtr;


    // enter and leaving window
    if (event.GetEventType() == wxEVT_LEAVE_WINDOW)
    {
        this->SetCursor(*wxSTANDARD_CURSOR);
        event.Skip();
        return;
    }

    if (event.GetEventType() == wxEVT_ENTER_WINDOW)
    {
        if (event.m_leftDown)
            this->SetCursor( wxCURSOR_HAND );
    }

    // checking bitmap and position
    if (!m_bitmap || ( event.m_x > m_bitmap->GetWidth() ) || (event.m_y > m_bitmap->GetHeight() ) )
    {
        event.Skip();
        return;
    }


    if (event.m_middleDown)
    {
        if (!event.Dragging() && !event.m_wheelRotation)
        {
            this->SynchronizeScroll( event.m_x, event.m_y );
            wxGetApp().Yield();
            controller->DrawCircles( );
            this->SetFocus();
            event.Skip();
        }
        return;
    }
    if (event.GetEventType() == wxEVT_MIDDLE_UP)
    {        
        controller->DrawCircles( true );
        this->SetFocus();
    }

    // left up and down
    if (event.GetEventType() == wxEVT_LEFT_UP)
    {
        this->SetCursor( *wxSTANDARD_CURSOR );
    }

    if (event.GetEventType() == wxEVT_LEFT_DOWN)
    {
        this->SetCursor( wxCURSOR_HAND );
        this->SynchronizeScroll( event.m_x, event.m_y );
    }

    // dragging with left down
    if ( event.Dragging() && event.m_leftDown)
    {
        this->SynchronizeScroll( event.m_x, event.m_y );
    }
    event.Skip();
}

//----------------------------------------------------------------------------
// SupImController
//----------------------------------------------------------------------------

IMPLEMENT_CLASS(SupImController,AxImageController)


BEGIN_EVENT_TABLE(SupImController,AxImageController)
END_EVENT_TABLE()

SupImController::SupImController( wxWindow *parent, wxWindowID id,
    const wxPoint &position, const wxSize& size, long style , int flags) :
    AxImageController( parent, id, position, size, style, flags )
{
    // m_redIm / m_greenIm are default-constructed cv::Mat (empty).
    m_imControl1Ptr = NULL;
    m_imControl2Ptr = NULL;
    m_viewSrc1Ptr = NULL;
    m_viewSrc2Ptr = NULL;
    m_supFilePtr = NULL;

    m_redBrightness = 0;
    m_redContrast = 0;
    m_greenBrightness = 0;
    m_greenContrast = 0;
}

SupImController::SupImController()
{
}

SupImController::~SupImController()
{
    // cv::Mat cleans itself up.
}

void SupImController::SetControllers( AxImageController *controller1, AxImageController *controller2 )
{
    m_imControl1Ptr = controller1;
    m_imControl2Ptr = controller2;
}

void SupImController::SetViews( SupImSrcWindow *view1, SupImSrcWindow *view2 )
{
    m_viewSrc1Ptr = view1;
    m_viewSrc2Ptr = view2;
}

void SupImController::ResetImage( AxImage image )
{
    AxImageController::ResetImage( image );

	if ( !m_imControl1Ptr || !m_imControl2Ptr )
		return;

    wxGetApp().AxBeginBusyCursor();

    // Pull the current wxImage/AxImage into a BGR cv::Mat (top-left origin
    // after the vertical flip GetCvMat applies, matching the old IM_RGB
    // buffer layout).
    cv::Mat bgr = GetCvMat( this );

    // Store the R and G channels for later brightness compositing.
    // OpenCV BGR: channel 0=B, 1=G, 2=R — the old IM code stored
    // data[0] as red and data[1] as green.
    std::vector<cv::Mat> planes;
    cv::split( bgr, planes );
    m_redIm = planes[2].clone();
    m_greenIm = planes[1].clone();

    wxGetApp().AxEndBusyCursor();

}

void SupImController::UpdateBrightness( )
{
    if ( m_redIm.empty() || m_greenIm.empty() )
        return;

	if ( !m_imControl1Ptr || !m_imControl2Ptr )
		return;

    wxASSERT_MSG(m_viewPtr,"View cannot be NULL");

    wxGetApp().AxBeginBusyCursor();

    // Working buffers: r_buf/g_buf accumulate brightness/contrast-adjusted
    // planes; imTmp is scratch. Replaces the old IM_GAMUT_BRIGHTCONT tone
    // adjust + IM_BIT_OR/IM_BIT_AND combines with cv::convertTo and
    // cv::bitwise_or / cv::bitwise_and.
    cv::Mat r_buf = m_redIm.clone();
    cv::Mat g_buf = m_greenIm.clone();
    cv::Mat imTmp;

    // IM's IM_GAMUT_BRIGHTCONT with parameters (brightness_pct, contrast_pct)
    // is a linear scale/offset:
    //   contrast_factor = 1 + contrast_pct / 100  (== tan of a rotation)
    //   out = clamp((in - 128) * contrast_factor + 128 + brightness_pct * 2.55, 0, 255)
    // The old code multiplied the UI sliders by 5.0 before passing them in.
    auto apply_bright_contrast = [](const cv::Mat &src, cv::Mat &dst,
                                    double brightness_pct, double contrast_pct)
    {
        double alpha = 1.0 + contrast_pct / 100.0;
        double beta  = 128.0 * (1.0 - alpha) + brightness_pct * 2.55;
        src.convertTo(dst, CV_8U, alpha, beta);
    };

    if ( (m_greenBrightness != 0) || (m_greenContrast != 0) )
    {
        double b = 5.0 * (double)m_greenBrightness;
        double c = 5.0 * (double)m_greenContrast;
        apply_bright_contrast( r_buf, imTmp, b, c );
        cv::bitwise_or ( r_buf, g_buf, r_buf ); // valeurs communes doivent rester à 100%
        cv::bitwise_and( imTmp, r_buf, r_buf ); // AND entre valeurs communes et brightness ajusté
    }
    if ( (m_redBrightness != 0) || (m_redContrast != 0) )
    {
        double b = 5.0 * (double)m_redBrightness;
        double c = 5.0 * (double)m_redContrast;
        apply_bright_contrast( g_buf, imTmp, b, c );
        cv::bitwise_or ( g_buf, r_buf, g_buf );
        cv::bitwise_and( imTmp, g_buf, g_buf );
    }
    cv::bitwise_and( r_buf, g_buf, imTmp );

    // Build a 3-channel BGR display image. The old IM code laid this out
    // as planar RGB: data[0]=R=r_buf, data[1]=G=g_buf, data[2]=B=imTmp.
    // OpenCV BGR channel order is (B, G, R) — so merge as (imTmp, g_buf,
    // r_buf) to preserve the same pixel colours.
    std::vector<cv::Mat> planes = { imTmp, g_buf, r_buf };
    cv::Mat im1_bgr;
    cv::merge( planes, im1_bgr );

    SetCvMat( this, im1_bgr );

    m_viewPtr->UpdateView();
    wxGetApp().AxEndBusyCursor();
}

void SupImController::ScrollSources( double x, double y )
{
	if ( !m_imControl1Ptr || !m_imControl2Ptr )
		return;

    wxASSERT_MSG( m_viewSrc1Ptr, "View 1 cannot be NULL");
    wxASSERT_MSG( m_viewSrc2Ptr, "View 2 cannot be NULL");

    m_viewSrc1Ptr->ScrollSource( x, y );
    m_viewSrc2Ptr->ScrollSource( x, y );
}

void SupImController::DrawCircles( bool clear )
{
	if ( !m_imControl1Ptr || !m_imControl2Ptr )
		return;

    wxASSERT_MSG( m_viewSrc1Ptr, "View 1 cannot be NULL");
    wxASSERT_MSG( m_viewSrc2Ptr, "View 2 cannot be NULL");

    if ( !clear )
    {
        m_viewSrc1Ptr->DrawCircle( );
        m_viewSrc2Ptr->DrawCircle( );
    }
    else
    {
        m_viewSrc1Ptr->RedrawBuffer( );
        m_viewSrc2Ptr->RedrawBuffer( );
    }
}

imPoint SupImController::ToLogical( wxPoint p )
{
    wxASSERT( this->IsOk() );
    
    return imPoint( p.x, this->GetHeight() - p.y );
}

wxPoint SupImController::ToRender( imPoint p )
{
    wxASSERT( this->IsOk() );
    
    return wxPoint( p.x, this->GetHeight() - p.y );

}


void SupImController::CloseDraggingSelection(wxPoint start, wxPoint end)
{
    wxASSERT( m_supFilePtr );
    
    if ( m_supFilePtr->IsSuperimposed() ) {
        return;
    }
    
    imPoint *points;
    if ( this->GetId() == ID2_CONTROLLER1) {
        points = m_supFilePtr->m_points1;
    }
    else {
        points = m_supFilePtr->m_points2; 
    }

    if (( end.x < this->GetWidth() / 2) && ( end.y < this->GetHeight() / 2 )) {
        //wxLogDebug("top left");
        points[1] = ToLogical(end);
    }
    else if (( end.x > this->GetWidth() / 2) && ( end.y < this->GetHeight() / 2 )) {
        //wxLogDebug("top right");
        points[3] = ToLogical(end);
    }
    else if (( end.x < this->GetWidth() / 2) && ( end.y > this->GetHeight() / 2 )) {
        //wxLogDebug("bottom left");
        points[0] = ToLogical(end);
    }
    else if (( end.x > this->GetWidth() / 2) && ( end.y > this->GetHeight() / 2 )) {
        //wxLogDebug("bottom right");
        points[2] = ToLogical(end);
    }
    m_viewPtr->UpdateViewFast();
}

void SupImController::SetInitialPoints()
{
    wxASSERT( m_supFilePtr );

    imPoint *points;
    if ( this->GetId() == ID2_CONTROLLER1) {
        points = m_supFilePtr->m_points1;
    }
    else {
        points = m_supFilePtr->m_points2; 
    }
    
    if ( !this->Ok() || !this->HasFilename() )
        return;
    
    int margin = 40;
    points[0] = imPoint( margin, margin );
    points[1] = imPoint( margin, this->GetHeight() - margin );
    points[2] = imPoint( GetWidth() - margin, margin );
    points[3] = imPoint( GetWidth() - margin, this->GetHeight() - margin );
}





#endif // AX_SUPERIMPOSITION

