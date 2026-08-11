/////////////////////////////////////////////////////////////////////////////
// Name:        imregister.cpp
// Author:      Laurent Pugin
// Created:     2004
// Copyright (c) Authors and others. All rights reserved.   
/////////////////////////////////////////////////////////////////////////////

#include <algorithm>
using std::min;
using std::max;

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#include "wx/file.h"

#include <opencv2/imgproc.hpp>

#include "image_ops.h"
#include "imregister.h"
#include "impage.h"
#include "imstaff.h"

#include "app/axapp.h"

#include "superimposition/sup.h" // not optimal, should be restructured...


// from  tiff.h, not very good but I don't want to include the file....
#define      PHOTOMETRIC_MINISWHITE      0       /* min value is white */
#define      PHOTOMETRIC_MINISBLACK      1       /* min value is black */
#define      PHOTOMETRIC_RGB             2       /* RGB color model */


//----------------------------------------------------------------------------
// ImRegister
//----------------------------------------------------------------------------

ImRegister::ImRegister( wxString path, bool *isModified ) :
    ImOperator( )
{
	m_path = path;
	m_isModified = 	isModified;
    Clear( );

	// aditional temporary images (default-constructed empty)

	m_imPage1Ptr = new ImPage( m_path );
	m_imPage2Ptr = new ImPage( m_path );
}

ImRegister::~ImRegister()
{
	ImageDestroy( m_src1 );
	ImageDestroy( m_src2 );
	ImageDestroy( m_result );
	if ( m_imPage1Ptr )
		delete m_imPage1Ptr;
	if ( m_imPage2Ptr )
		delete m_imPage2Ptr;
}

bool ImRegister::Terminate( int code,  ... )
{
    // Attention que deux de ces pointeurs ne refere pas la meme adresse lors de l'appel de cette methode !
    ImageDestroy( m_im1 );
    ImageDestroy( m_im2 );

	wxLogDebug("Terminate::ImRegister");

	va_list argptr;
    va_start( argptr, code );
	return ImOperator::Terminate( code, argptr );
}

void ImRegister::Clear( )
{
	ImageDestroy( m_src1 );
	ImageDestroy( m_src2 );
	ImageDestroy( m_result );
}

bool ImRegister::Load( TiXmlElement *file_root )
{
	if ( !file_root )
		return false;

	this->Clear();
	bool failed = false;

    if ( !failed )
		failed = (!wxFileExists( m_path + "src1.tif" ) || !Read( m_path + "src1.tif", m_src1, 0 ));

    if ( !failed )
		failed = (!wxFileExists( m_path + "src2.tif" ) || !Read( m_path + "src2.tif", m_src2, 0 ));

    if ( !failed )
		failed = (!wxFileExists( m_path + "result.tif" ) || !Read( m_path + "result.tif", m_result, 0 ));

	/*  load more data ?
    TiXmlElement *root = NULL;
    TiXmlNode *node = NULL;
    TiXmlElement *elem = NULL;
	
	node = file_root->FirstChild( "imregister" );
	if ( !node ) return false;
		
	root = node->ToElement();
    if ( !root ) return false;

    if ( root->Attribute("skew"))
        m_skew = atof(root->Attribute("skew"));

	// needed ???
    if ( root->Attribute("resize"))
        m_resize = atof(root->Attribute("resize"));
	
	// needed ???
    if ( root->Attribute("resized"))
        m_resized = atof(root->Attribute("resized"));

    if ( root->Attribute("reduction"))
        m_reduction = atoi(root->Attribute("reduction"));

    if ( root->Attribute("x2"))
        m_x2 = atoi(root->Attribute("x2"));

    if ( root->Attribute("x1"))
        m_x1 = atoi(root->Attribute("x1"));

    if ( root->Attribute("width"))
        m_size.SetWidth( atoi(root->Attribute("width")) );

    if ( root->Attribute("height"))
        m_size.SetHeight( atoi(root->Attribute("height")) );

    if ( root->Attribute("line_width"))
        m_line_width = atoi(root->Attribute("line_width"));

    if ( root->Attribute("space_width"))
        m_space_width = atoi(root->Attribute("space_width"));

    if ( root->Attribute("staff_height"))
        m_staff_height = atoi(root->Attribute("staff_height"));

    // staves
    for( node = root->FirstChild( "staff" ); node; node = node->NextSibling( "staff" ) )
    {
        elem = node->ToElement();
        if (!elem) 
			return false;
        ImStaff imstaff;
        imstaff.Load( elem );
        m_staves.Add( imstaff );
    }
	*/
    
    return !failed;
}

bool ImRegister::Save( TiXmlElement *file_root )
{
	bool failed = false;

    if ( !failed && !m_src1.empty() )
		failed = !Write( m_path + "src1.tif", m_src1 );

    if ( !failed && !m_src2.empty() )
		failed = !Write( m_path + "src2.tif", m_src2 );

    if ( !failed && !m_result.empty() )
		failed = !Write( m_path + "result.tif", m_result );
	
	/* write more data ?
    wxString tmp;

    TiXmlElement root("impage");
    
    tmp = wxString::Format("%d", m_reduction );
    root.SetAttribute( "reduction",  tmp.c_str() );

    tmp = wxString::Format("%f", m_skew );
    root.SetAttribute( "skew", tmp.c_str() );
	
    tmp = wxString::Format("%f", m_resized );
    root.SetAttribute( "resized", tmp.c_str() );

    tmp = wxString::Format("%d", m_x1 );
    root.SetAttribute( "x1", tmp.c_str() );

    tmp = wxString::Format("%d", m_x2 );
    root.SetAttribute( "x2", tmp.c_str() );

    tmp = wxString::Format("%d", m_size.GetWidth() );
    root.SetAttribute( "width", tmp.c_str() );

    tmp = wxString::Format("%d", m_size.GetHeight() );
    root.SetAttribute( "height", tmp.c_str() );

    tmp = wxString::Format("%d", m_line_width );
    root.SetAttribute( "line_width", tmp.c_str() );

    tmp = wxString::Format("%d", m_space_width );
    root.SetAttribute( "space_width",  tmp.c_str() );

    tmp = wxString::Format("%d", m_staff_height );
    root.SetAttribute( "staff_height",  tmp.c_str() );

    for( int i = 0; i < (int)m_staves.GetCount() ; i++)
    {
        TiXmlElement elem ("staff");
        m_staves[i].Save( &elem );
        root.InsertEndChild( elem );
    }

	if ( file_root )
	    file_root->InsertEndChild( root );
	*/
	
    return !failed;
}


bool ImRegister::Init( wxString filename1, wxString filename2 )
{
    wxASSERT_MSG( m_progressDlg, "Progress dialog cannot be NULL");
	wxASSERT_MSG( m_imPage1Ptr, "ImPage1 cannot be NULL" );
	wxASSERT_MSG( m_imPage2Ptr, "ImPage2 cannot be NULL" );
	
    if (!m_progressDlg->SetOperation( _("Checking the image 1 ...") ) )
        return this->Terminate( ERR_CANCELED );

	m_imPage1Ptr->SetProgressDlg( m_progressDlg );
	
	if ( !m_imPage1Ptr->Check( filename1, 5000, 1200 ) )
		return false;
        
    if (!m_progressDlg->SetOperation( _("Checking the image 2 ...") ) )
        return this->Terminate( ERR_CANCELED );

	m_imPage2Ptr->SetProgressDlg( m_progressDlg );
	
	if ( !m_imPage2Ptr->Check( filename2, 5000, 1200 ) )
		return false;

	ImageDestroy( m_src1 );
	ImageDestroy( m_src2 );

    m_src1 = m_imPage1Ptr->m_img0.clone();
    if ( m_src1.empty() )
        return this->Terminate( ERR_MEMORY );
	ImageDestroy( m_src2 );
    m_src2 = m_imPage2Ptr->m_img0.clone();
    if ( m_src2.empty() )
        return this->Terminate( ERR_MEMORY );

	if ( m_isModified ) 
		*m_isModified = true;
	return this->Terminate( ERR_NONE );
}



imPoint ImRegister::CalcPositionAfterRotation( imPoint point , float rot_alpha, 
                                  int w, int h, int new_w, int new_h)
{

    imPoint center( w/2, h/2);
    int distCenterX = point.x - center.x;
    int distCenterY = point.y - center.y;
    // pythagore, distance entre le point d'origine et le centre
    int distCenter = (int)sqrt( pow( (double)distCenterX, 2 ) + pow( (double)distCenterY, 2 ) );

    // angle d'origine entre l'axe x et la droite passant par le point et le centre
    float alpha = atan ( (float)distCenterX / (float)(distCenterY) );
    
    imPoint new_p = center;
    int new_distCenterX, new_distCenterY;

    new_distCenterX = abs( (int)( sin( alpha - rot_alpha ) * distCenter ) );
    new_p.x += (center.x > point.x) ? -new_distCenterX : new_distCenterX;

    new_distCenterY = abs( (int)( cos( alpha - rot_alpha ) * distCenter ) );
    new_p.y += (center.y > point.y) ? -new_distCenterY : new_distCenterY;
    
    // marges
    new_p.x += (new_w - w) / 2;
    new_p.y += (new_h - h) / 2;

    // verifier la position du point -> TODO traitement d'erreur
    if ((new_p.x < 0) || (new_p.x > new_w))
        new_p = point;
    else if ((new_p.y < 0) || (new_p.y > new_h))
        new_p = point;

    return new_p;
}

bool ImRegister::DetectPoints( imPoint *points1, imPoint *points2)
{
    wxASSERT_MSG( m_progressDlg, "Progress dialog cannot be NULL");
	wxASSERT_MSG( !m_src1.empty(), "Src1 cannot be NULL");
	wxASSERT_MSG( !m_src2.empty(), "Src2 cannot be NULL");
	wxASSERT_MSG( m_imPage1Ptr, "ImPage1 cannot be NULL" );
	wxASSERT_MSG( m_imPage2Ptr, "ImPage2 cannot be NULL" );

	bool failed = false;

    if (!m_progressDlg->SetOperation( _("Analyzing the image 1 ...") ) )
        return this->Terminate( ERR_CANCELED );
	
	if ( !failed ) 
        failed = !m_imPage1Ptr->Deskew( 10.0 ); // 2 operations max
        
    if ( !failed ) 
        failed = !m_imPage1Ptr->FindStaves(  3, 50, false, false );  // 4 operations max
	
    if (!m_progressDlg->SetOperation( _("Analyzing the image 2 ...") ) )
        return this->Terminate( ERR_CANCELED );
    
	if ( !failed ) 
        failed = !m_imPage2Ptr->Deskew( 10.0 ); // 2 operations max
        
    if ( !failed ) 
        failed = !m_imPage2Ptr->FindStaves(  3, 50, false, false );  // 4 operations max
		
	if ( failed )
		return this->Terminate( ERR_UNKNOWN );
	
	// in case the number of staves is not identical, we register from top one
	// as staves order is reversed, we have to detect the index of the bottom one as it is going
	// to be different from 0 for one of the pages
	int staff_nb = min( (int)m_imPage1Ptr->m_staves.GetCount(), (int)m_imPage2Ptr->m_staves.GetCount() );
	int bottom_staff1 = (int)m_imPage1Ptr->m_staves.GetCount() - staff_nb;
	int bottom_staff2 = (int)m_imPage2Ptr->m_staves.GetCount() - staff_nb;
	int top_staff1 = staff_nb - 1 + bottom_staff1;
	int top_staff2 = staff_nb - 1 + bottom_staff2;
	
	int top_y1 = m_imPage1Ptr->m_staves[top_staff1].m_y;
	int top_y2 = m_imPage2Ptr->m_staves[top_staff2].m_y;
	int bottom_y1 = m_imPage1Ptr->m_staves[bottom_staff1].m_y;
	int bottom_y2 = m_imPage2Ptr->m_staves[bottom_staff2].m_y;
		
	// y is the center position off the staff. We want to get the top line of the first staff 
	// and the bottom line of the last as we will get better registration results
	if ( ( top_y1 - (STAFF_HEIGHT / (2 * m_imPage1Ptr->m_resize) ) > 0 ) 
		&& ( top_y2 - (STAFF_HEIGHT / (2 * m_imPage2Ptr->m_resize) ) > 0 ) )
	{
		top_y1 -= STAFF_HEIGHT / (2 * m_imPage1Ptr->m_resize);
		top_y2 -= STAFF_HEIGHT / (2 * m_imPage2Ptr->m_resize);
	}
	if ( ( bottom_y1 + (STAFF_HEIGHT / (2 * m_imPage1Ptr->m_resize) ) < m_imPage1Ptr->m_img1.rows )
		&& ( bottom_y2 + (STAFF_HEIGHT / (2 * m_imPage2Ptr->m_resize) ) < m_imPage2Ptr->m_img1.rows ) )
	{
		bottom_y1 += STAFF_HEIGHT / (2 * m_imPage1Ptr->m_resize);
		bottom_y2 += STAFF_HEIGHT / (2 * m_imPage2Ptr->m_resize);
	}
	
	
	points1[1] = imPoint( m_imPage1Ptr->m_x1, bottom_y1 );
	points1[3] = imPoint( m_imPage1Ptr->m_x2, bottom_y1 );
	points1[0] = imPoint( m_imPage1Ptr->m_x1, top_y1 );
	points1[2] = imPoint( m_imPage1Ptr->m_x2, top_y1 );

	points2[1] = imPoint( m_imPage2Ptr->m_x1, bottom_y2 );
	points2[3] = imPoint( m_imPage2Ptr->m_x2, bottom_y2 );
	points2[0] = imPoint( m_imPage2Ptr->m_x1, top_y2 );
	points2[2] = imPoint( m_imPage2Ptr->m_x2, top_y2 );
	
	ImageDestroy( m_src1 );
    m_src1 = m_imPage1Ptr->m_img1.clone();
    if ( m_src1.empty() )
        return this->Terminate( ERR_MEMORY );
	cv::bitwise_not( m_src1, m_src1 );

    ImageDestroy( m_src2 );
    m_src2 = m_imPage2Ptr->m_img1.clone();
    if ( m_src2.empty() )
        return this->Terminate( ERR_MEMORY );
	cv::bitwise_not( m_src2, m_src2 );
    
    if ( (int)m_imPage1Ptr->m_staves.GetCount() != (int)m_imPage2Ptr->m_staves.GetCount() )
	{
		wxLogError( "Staff detection did not retrieve the same number of staves of both images" );
		wxLogDebug("Page1, staves %d %d; Page2, staves %d %d", top_staff1, bottom_staff1, top_staff2, bottom_staff2 );
        return this->Terminate( ERR_UNKNOWN );
	}
    
    /*
    wxLogDebug("point1 1 %d %d", points1[0].x, points1[0].y );
    wxLogDebug("point1 2 %d %d", points1[1].x, points1[1].y );
    wxLogDebug("point1 3 %d %d", points1[2].x, points1[2].y );
    wxLogDebug("point1 4 %d %d", points1[3].x, points1[3].y );
    
    wxLogDebug("point2 1 %d %d", points2[0].x, points2[0].y );
    wxLogDebug("point2 2 %d %d", points2[1].x, points2[1].y );
    wxLogDebug("point2 3 %d %d", points2[2].x, points2[2].y );
    wxLogDebug("point2 4 %d %d", points2[3].x, points2[3].y );
    */
	
	return this->Terminate( ERR_NONE );	
}

#ifdef AX_SUPERIMPOSITION
bool ImRegister::Register( imPoint *points1, imPoint *points2)
{
    
    /*
    wxLogDebug("point1 1 %d %d", points1[0].x, points1[0].y );
    wxLogDebug("point1 2 %d %d", points1[1].x, points1[1].y );
    wxLogDebug("point1 3 %d %d", points1[2].x, points1[2].y );
    wxLogDebug("point1 4 %d %d", points1[3].x, points1[3].y );
     
    wxLogDebug("point2 1 %d %d", points2[0].x, points2[0].y );
    wxLogDebug("point2 2 %d %d", points2[1].x, points2[1].y );
    wxLogDebug("point2 3 %d %d", points2[2].x, points2[2].y );
    wxLogDebug("point2 4 %d %d", points2[3].x, points2[3].y );
    */
     
    
    wxASSERT_MSG( m_progressDlg, "Progress dialog cannot be NULL");
	wxASSERT_MSG( !m_src1.empty(), "Src1 cannot be NULL");
	wxASSERT_MSG( !m_src2.empty(), "Src2 cannot be NULL");

    if (!m_progressDlg->SetOperation( _("Registration ...") ) )
        return this->Terminate( ERR_CANCELED );

	int i;
    // copy m_reg_points locally for registration
	for (i = 0; i < 4; i++ )
	{
		m_reg_points1[i] = points1[i];
		m_reg_points2[i] = points2[i];
	}

    // calculer la hauteur des segments et redimensionner
    int left1 = (int)sqrt( 
        pow( (double)(m_reg_points1[0].x - m_reg_points1[1].x) , 2) + 
        pow( (double)(m_reg_points1[0].y - m_reg_points1[1].y), 2));
    int right1 = (int)sqrt( 
        pow( (double)(m_reg_points1[2].x - m_reg_points1[3].x) , 2) + 
        pow( (double)(m_reg_points1[2].y - m_reg_points1[3].y), 2));
	int vmean1 = (left1 + right1) / 2; // moyenne des deux cotes
    //wxLogDebug(" segment 1 %d", segment1 );
    int left2 = (int)sqrt(
        pow( (double)(m_reg_points2[0].x - m_reg_points2[1].x), 2) + 
        pow( (double)(m_reg_points2[0].y - m_reg_points2[1].y), 2));
    int right2 = (int)sqrt(
        pow( (double)(m_reg_points2[2].x - m_reg_points2[3].x), 2) + 
        pow( (double)(m_reg_points2[2].y - m_reg_points2[3].y), 2));
	int vmean2 = (left2 + right2) / 2; // moyenne des deux cotes
    // facteur de redimensionnement
    float vfactor1 = 1.0;
	float vfactor2 = vfactor1 *(float)vmean1 / (float)vmean2;
	
	
    // idem segments horizontaux
    int top1 = (int)sqrt( 
        pow( (double)(m_reg_points1[0].x - m_reg_points1[2].x) , 2) + 
        pow( (double)(m_reg_points1[0].y - m_reg_points1[2].y), 2));
    int bottom1 = (int)sqrt( 
        pow( (double)(m_reg_points1[1].x - m_reg_points1[3].x) , 2) + 
        pow( (double)(m_reg_points1[1].y - m_reg_points1[3].y), 2));
	int hmean1 = (top1 + bottom1) / 2; // moyenne des deux cotes
    //wxLogDebug(" segment 1 %d", segment1 );
    int top2 = (int)sqrt(
        pow( (double)(m_reg_points2[0].x - m_reg_points2[2].x), 2) + 
        pow( (double)(m_reg_points2[0].y - m_reg_points2[2].y), 2));
    int bottom2 = (int)sqrt(
        pow( (double)(m_reg_points2[1].x - m_reg_points2[3].x), 2) + 
        pow( (double)(m_reg_points2[1].y - m_reg_points2[3].y), 2));
	int hmean2 = (top2 + bottom2) / 2; // moyenne des deux cotes
    // facteur de redimensionnement, only for image 2 (the one for image 1 is identical to vfactor1
	float hfactor1 = vfactor1;
    float hfactor2 = hfactor1 * (float)hmean1 / (float)hmean2;
	
    //wxLogDebug("vmean1 %d, hmean1 %d, vmean2 %d, hmean2 %d", vmean1, hmean1, vmean2, hmean2);
	//wxLogDebug("vfactor1 %f, vfactor2 %f, hfactor1 %f, hfactor2 %f", vfactor1, vfactor2, hfactor1, hfactor2);

    if (!m_progressDlg->SetOperation( _("Preparartion of image 1 ...") ))
        return this->Terminate( ERR_CANCELED );

    m_im1 = m_src1.clone();
    if ( m_im1.empty() )
        return this->Terminate( ERR_MEMORY );

    // median filtering
    if (SupEnv::s_filter1)
    {
        if ( !m_progressDlg->SetOperation( _("Filtering image 1 ...") ))
            return this->Terminate( ERR_CANCELED );

        cv::medianBlur(m_im1, m_opImTmp1, 3);
        SwapImages( m_im1, m_opImTmp1 );

    }
    // resize
    /*
    if (!m_progressDlg->SetOperation( _("Resizing image ...") ))
        return this->Terminate( ERR_CANCELED );

    m_opImTmp1 = imImageCreate(  (int)(m_im1->width  * hfactor1), (int)(m_im1->height  * vfactor1),
            m_im1->color_space, m_im1->data_type);
    if ( !m_opImTmp1 )
        return this->Terminate( ERR_MEMORY );

    if ( !imProcessResize( m_im1 ,m_opImTmp1, SupEnv::s_interpolation) )
        return this->Terminate( ERR_CANCELED );

    SwapImages( &m_im1, &m_opImTmp1 );
    */

    // ajuster la position des m_reg_points
    m_reg_points1[0].x = (int)(m_reg_points1[0].x * hfactor1);
    m_reg_points1[0].y = (int)(m_reg_points1[0].y * vfactor1);
    m_reg_points1[1].x = (int)(m_reg_points1[1].x * hfactor1);
    m_reg_points1[1].y = (int)(m_reg_points1[1].y * vfactor1);
    m_reg_points1[2].x = (int)(m_reg_points1[2].x * hfactor1);
    m_reg_points1[2].y = (int)(m_reg_points1[2].y * vfactor1);
	m_reg_points1[3].x = (int)(m_reg_points1[3].x * hfactor1);
    m_reg_points1[3].y = (int)(m_reg_points1[3].y * vfactor1);


    if (!m_progressDlg->SetOperation( _("Preparation of image 2 ...") ))
        return this->Terminate( ERR_CANCELED );

    m_im2 = m_src2.clone();
    if ( m_im2.empty() )
        return this->Terminate( ERR_MEMORY );

    // median filtering
    if (SupEnv::s_filter2)
    {
        if (!m_progressDlg->SetOperation( _("Filtering image 2 ...") ))
            return this->Terminate( ERR_CANCELED );

        cv::medianBlur(m_im2, m_opImTmp1, 3);
        SwapImages( m_im2, m_opImTmp1 );
    }
    // resize
    if (!m_progressDlg->SetOperation( _("Resizing image 2 ...") ))
        return this->Terminate( ERR_CANCELED );

    {
        // IM's imProcessResize order arg: 0=nearest, 1=linear, 2=cubic.
        int cv_interp = SupEnv::s_interpolation == 0 ? cv::INTER_NEAREST
                      : SupEnv::s_interpolation == 1 ? cv::INTER_LINEAR
                      :                                cv::INTER_CUBIC;
        cv::resize(m_im2, m_opImTmp1,
                   cv::Size((int)(m_im2.cols * hfactor2), (int)(m_im2.rows * vfactor2)),
                   0, 0, cv_interp);
    }
    SwapImages( m_im2, m_opImTmp1 );

    m_reg_points2[0].x = (int)(m_reg_points2[0].x * hfactor2);
    m_reg_points2[0].y = (int)(m_reg_points2[0].y * vfactor2);
    m_reg_points2[1].x = (int)(m_reg_points2[1].x * hfactor2);
    m_reg_points2[1].y = (int)(m_reg_points2[1].y * vfactor2);
    m_reg_points2[2].x = (int)(m_reg_points2[2].x * hfactor2);
    m_reg_points2[2].y = (int)(m_reg_points2[2].y * vfactor2);
	m_reg_points2[3].x = (int)(m_reg_points2[3].x * hfactor2);
    m_reg_points2[3].y = (int)(m_reg_points2[3].y * vfactor2);

    // calculer les angles et pivoter
    int new_w, new_h;
    double cos0, sin0;

    // image 1
    // calculer l'angle
    if (!m_progressDlg->SetOperation( _("Rotation of image 1 ...") ))
        return this->Terminate( ERR_CANCELED );

    float left_alpha1 = float(m_reg_points1[0].x - m_reg_points1[1].x) / float(m_reg_points1[0].y - m_reg_points1[1].y);
    left_alpha1 = atan(left_alpha1);
	float right_alpha1 = float(m_reg_points1[2].x - m_reg_points1[3].x) / float(m_reg_points1[2].y - m_reg_points1[3].y);
    right_alpha1 = atan(right_alpha1);
	float alpha1 = -(left_alpha1 + right_alpha1) / 2;

    sin0 = sin(alpha1);
    cos0 = cos(alpha1);
    ax::calc_rotate_size( m_im1.cols, m_im1.rows, &new_w, &new_h, cos0, sin0 );
    // ajuster la position des m_reg_points
    m_reg_points1[0] = CalcPositionAfterRotation( m_reg_points1[0], alpha1, m_im1.cols, m_im1.rows, new_w, new_h);
    m_reg_points1[1] = CalcPositionAfterRotation( m_reg_points1[1], alpha1, m_im1.cols, m_im1.rows, new_w, new_h);
    m_reg_points1[2] = CalcPositionAfterRotation( m_reg_points1[2], alpha1, m_im1.cols, m_im1.rows, new_w, new_h);

    ax::rotate_center( m_im1, m_opImTmp1, new_w, new_h, cos0, sin0,
                       SupEnv::s_interpolation );
    SwapImages( m_im1, m_opImTmp1 );


    // idem image 2
    if (!m_progressDlg->SetOperation( _("Rotation of image 2 ...") ))
        return this->Terminate( ERR_CANCELED );

    float left_alpha2 = float(m_reg_points2[0].x - m_reg_points2[1].x) / float(m_reg_points2[0].y - m_reg_points2[1].y);
    left_alpha2 = atan( left_alpha2 );
    float right_alpha2 = float(m_reg_points2[2].x - m_reg_points2[3].x) / float(m_reg_points2[2].y - m_reg_points2[3].y);
    right_alpha2 = atan( right_alpha2 );
	float alpha2 = -(left_alpha2 + right_alpha2) / 2;

    sin0 = sin(alpha2);
    cos0 = cos(alpha2);
    ax::calc_rotate_size( m_im2.cols, m_im2.rows, &new_w, &new_h, cos0, sin0 );
    m_reg_points2[0] = CalcPositionAfterRotation( m_reg_points2[0], alpha2, m_im2.cols, m_im2.rows, new_w, new_h);
    m_reg_points2[1] = CalcPositionAfterRotation( m_reg_points2[1], alpha2, m_im2.cols, m_im2.rows, new_w, new_h);
    m_reg_points2[2] = CalcPositionAfterRotation( m_reg_points2[2], alpha2, m_im2.cols, m_im2.rows, new_w, new_h);

    ax::rotate_center( m_im2, m_opImTmp1, new_w, new_h, cos0, sin0,
                       SupEnv::s_interpolation );
    SwapImages( m_im2, m_opImTmp1 );

    // deplacer (crop ou marges)
    if (!m_progressDlg->SetOperation( _("Calculation of margins ...") ) )
       return this->Terminate( ERR_CANCELED );

    //wxLogDebug("p1 %d %d; p2 %d %d; p3 %d %d", m_reg_points1[0].x, m_reg_points1[0].y, m_reg_points1[1].x , m_reg_points1[1].y , m_reg_points1[2].x , m_reg_points1[2].y);
    //wxLogDebug("p1 %d %d; p2 %d %d; p3 %d %d", m_reg_points2[0].x, m_reg_points2[0].y, m_reg_points2[1].x , m_reg_points2[1].y , m_reg_points2[2].x , m_reg_points2[2].y);


    // marges x
    int im1_mx1 = min( m_reg_points1[0].x, m_reg_points1[1].x );
    int im2_mx1 = min( m_reg_points2[0].x, m_reg_points2[1].x );
    int im1_mx2 = m_reg_points1[2].x;
    int im2_mx2 = m_reg_points2[2].x;
    // marges y
    int im1_my1 = m_reg_points1[0].y;
    int im2_my1 = m_reg_points2[0].y;
    int im1_my2 = max( m_reg_points1[1].y, m_reg_points1[2].y );
    int im2_my2 = max( m_reg_points2[1].y, m_reg_points2[2].y );

    // min de crop des 2 images
    int minx1 = (im1_mx1 > im2_mx1 ) ? im1_mx1 - im2_mx1 : 0;
    int minx2 = (im2_mx1 > im1_mx1 ) ? im2_mx1 - im1_mx1 : 0;
    int miny1 = (im1_my1 > im2_my1 ) ? im1_my1 - im2_my1 : 0;
    int miny2 = (im2_my1 > im1_my1 ) ? im2_my1 - im1_my1 : 0;

    // largeur et hauteur de la zone de superposition + point d'orgine
    int width = min( ( im1_mx2 - im1_mx1 ), ( im2_mx2 - im2_mx1 ) );
    int height = min( ( im1_my2 - im1_my1 ), ( im2_my2 - im2_my1 ) );
    imPoint origine( min ( im1_mx1, im2_mx1 ), min ( im1_my1, im2_my1 ) );

    // largeur et hauteur des nouvelles images (zone de superposition + marge minimale )
    int im_width = origine.x + width + min( m_im1.cols - im1_mx1 - width, m_im2.cols - im2_mx1 - width );
    int im_height = origine.y + height + min( m_im1.rows - im1_my1 - height, m_im2.rows - im2_my1 - height );

    m_opImTmp1 = cv::Mat(im_height, im_width, CV_8UC1);
    if ( m_opImTmp1.empty() )
        return this->Terminate( ERR_MEMORY );

    m_im1(cv::Rect(minx1, miny1, m_opImTmp1.cols, m_opImTmp1.rows)).copyTo(m_opImTmp1);
    SwapImages( m_im1, m_opImTmp1 );

    m_opImTmp1 = cv::Mat(im_height, im_width, CV_8UC1);
    if ( m_opImTmp1.empty() )
        return this->Terminate( ERR_MEMORY );

    m_im2(cv::Rect(minx2, miny2, m_opImTmp1.cols, m_opImTmp1.rows)).copyTo(m_opImTmp1);
    SwapImages( m_im2, m_opImTmp1 );

    // garder l'image original pour le fichier
	ImageDestroy( m_src1 );
    m_src1 = m_im1.clone();
    if ( m_src1.empty() )
        return this->Terminate( ERR_MEMORY );
	cv::bitwise_not( m_im1, m_src1 );
	ImageDestroy( m_src2 );
    m_src2 = m_im2.clone();
    if ( m_src2.empty() )
        return this->Terminate( ERR_MEMORY );
	cv::bitwise_not( m_im2, m_src2 );
    
    // we can start with a fairly wide window
    imSize window( max( SupEnv::s_corr_x, width / 25 ), max( SupEnv::s_corr_y, height / 25 ) );
	wxLogDebug( "Window %d x %d", window.GetWidth(), window.GetHeight() );

    m_opImAlign = cv::Mat( m_im2.rows + 2 * window.GetHeight(), m_im2.cols + 2 * window.GetWidth(), CV_8UC1 );
    if ( m_opImAlign.empty() )
        return this->Terminate( ERR_MEMORY );

    m_opImAlign.setTo( 255 );
    if ( m_im1.total() < m_opImAlign.total() )
    {
        m_im1.copyTo( m_opImAlign(cv::Rect(window.GetWidth(), window.GetHeight(), m_im1.cols, m_im1.rows)) );
    }
	
    int c;
    m_sub_register_total = 1;
    for ( c = 0; c <= SupEnv::s_subWindowLevel; c++ ) {
        m_sub_register_total += pow( pow( 2, c + 1 ), 2 );
    }
    
    if (!m_progressDlg->SetOperation( _("Image registration ..." ) ) )
       return this->Terminate( ERR_CANCELED );
    
	m_counter = imCounterBegin("Image registration");
	imCounterTotal(m_counter, m_sub_register_total, "Image registration");
    
    if ( !SubRegister( origine, window, imSize( width, height ), 0, 1, 1 ) )
            return this->Terminate( ERR_CANCELED );
    
    imCounterEnd( m_counter );
    m_opImAlign(cv::Rect(window.GetWidth(), window.GetHeight(), m_im2.cols, m_im2.rows)).copyTo(m_im2);
    ImageDestroy( m_opImAlign );

    if (!m_progressDlg->SetOperation( _("Writing image on disk ...") ) )
        return this->Terminate( ERR_CANCELED );

	// image for negative and bitwise operation
    m_opImAlign = cv::Mat(im_height, im_width, CV_8UC1);
    if ( m_opImAlign.empty() )
        return this->Terminate( ERR_MEMORY );

    // superposition in result imge
	ImageDestroy( m_result );
    // IM_RGB stored planes 0=R, 1=G, 2=B; OpenCV stores BGR interleaved.
    // Compose channels first, then merge into m_result as BGR.
    cv::Mat r_neg, g_neg, b_xor;
    cv::bitwise_not( m_im1, r_neg );
    cv::bitwise_not( m_im2, g_neg );
    cv::bitwise_xor( m_im1, m_im2, m_opImAlign );
    b_xor = m_opImAlign;
    cv::merge( std::vector<cv::Mat>{ b_xor, g_neg, r_neg }, m_result );

	if ( m_isModified )
		*m_isModified = true;

    return this->Terminate( ERR_NONE );
}
#endif

#ifdef AX_SUPERIMPOSITION
bool ImRegister::SubRegister( imPoint origine, imSize window, imSize size, int level, int row, int column )
{
    
	int x = 0, y = 0, maxCorr;

    imSize subwindow = window;

    if (!ax::safe_crop(m_im1, &size.x, &size.y, &origine.x, &origine.y))
        return this->Terminate( ERR_UNKNOWN );

    m_opImTmp1 = cv::Mat( size.GetHeight(), size.GetWidth(), CV_8UC1 );
    if ( m_opImTmp1.empty() )
        return this->Terminate( ERR_MEMORY );

    m_opImTmp2 = cv::Mat( size.GetHeight(), size.GetWidth(), CV_8UC1 );
    if ( m_opImTmp2.empty() )
        return this->Terminate( ERR_MEMORY );

    m_im1(cv::Rect(origine.x, origine.y, m_opImTmp1.cols, m_opImTmp1.rows)).copyTo(m_opImTmp1);
    m_im2(cv::Rect(origine.x, origine.y, m_opImTmp2.cols, m_opImTmp2.rows)).copyTo(m_opImTmp2);

    m_progressDlg->SuspendCounter();
    DistByCorrelation( m_opImTmp1, m_opImTmp2, window, &x, &y, &maxCorr );
    m_progressDlg->ReactiveCounter();

    if (!imCounterInc(m_counter))
        return false;

    ImageDestroy( m_opImTmp1 );
    ImageDestroy( m_opImTmp2 );

    //wxLogDebug( "Correlation decalage %d %d", x, y );
    
    // use the detected shift to determine the next window size (but at least 5 pixels)
    subwindow = imSize( max(abs(3*x), SupEnv::s_split_x), max(abs(3*y), SupEnv::s_split_y) );

	if ( level > SupEnv::s_subWindowLevel )
    // end of the recursion
    {  
        int pos_x = origine.x;
        int pos_y = origine.y;
        int width = size.GetWidth();
        int height = size.GetHeight();
        
        int plevel = pow( 2, level );
        
        if ((column / plevel) == 1)
            width = 100000; // extreme value, assume to be more than the maximum
        if ((row / plevel) == 1)
            height = 100000;
        if (column == 1) {
            width += origine.x;
            pos_x = 0;
        }
        if (row == 1)
        {
            height += origine.y;
            pos_y = 0; 
        }
        
        bool safe_ok = ax::safe_crop( m_im2, &width, &height, &pos_x, &pos_y );
        if ( safe_ok )
        {
            m_opImMask = cv::Mat( height, width, CV_8UC1 );
            if ( m_opImMask.empty() )
                return this->Terminate( ERR_MEMORY );

            //if (( row == 1 ) || ((row / plevel) == 1) || (column == 1) || ((column / plevel) == 1) ) // border only, for debug
            {
                m_im2(cv::Rect(pos_x, pos_y, m_opImMask.cols, m_opImMask.rows)).copyTo(m_opImMask);
                m_opImMask.copyTo( m_opImAlign(cv::Rect(
                    (m_opImAlign.cols - m_im2.cols) / 2 + pos_x - x,
                    (m_opImAlign.rows - m_im2.rows) / 2 + pos_y - y,
                    m_opImMask.cols, m_opImMask.rows)) );
            }
            ImageDestroy( m_opImMask );
        }

		return true;
	}
	
    // move the content, including the border if necessary
    int move_x = origine.x;
    int move_y = origine.y;
    int move_width = size.GetWidth();
    int move_height = size.GetHeight();
        
    int plevel = pow( 2, level );
    if ((column / plevel) == 1)
        move_width = 100000; // extreme value, assume to be more than the maximum
    if ((row / plevel) == 1)
        move_height = 100000; 
    if (column == 1) {
        move_width += origine.x;
        move_x = 0;
    }
    if (row == 1)
    {
        move_height += origine.y;
        move_y = 0; 
    }
    
    // this method return the maximum values for croping
    if (!ax::safe_crop( m_im2, &move_width, &move_height, &move_x, &move_y ) ) {
        return this->Terminate( ERR_UNKNOWN );
    }
    m_opImTmp1 = cv::Mat( move_height, move_width, CV_8UC1 );
	if ( m_opImTmp1.empty() )
        return this->Terminate( ERR_MEMORY );

    m_im2(cv::Rect(move_x, move_y, m_opImTmp1.cols, m_opImTmp1.rows)).copyTo(m_opImTmp1);
    // actually move the data
    m_opImTmp1.copyTo( m_im2(cv::Rect(move_x - x, move_y - y, m_opImTmp1.cols, m_opImTmp1.rows)) );
    ImageDestroy( m_opImTmp1 );

    // the problem here is that we have a recusion: we cannot use a m_opXXX image because it would be overriden
    // we use a local variable 'buffer' which would NOT be destroyed if a Terminate occur deeper in the recursion
    // => potential memory leak if the program keeps failing....
	cv::Mat buffer( size.GetHeight(), size.GetWidth(), CV_8UC1 );
	if ( buffer.empty() )
        return this->Terminate( ERR_MEMORY );

    m_im2(cv::Rect(origine.x - x, origine.y - y, buffer.cols, buffer.rows)).copyTo(buffer);
    
    
	origine.x -= x;
	origine.y -= y;

	// for subsize as we done what to loose pixels because of odd values
	imSize subsize1 = size / 2;
	imSize subsize2 = size - subsize1;
	imSize subsize3( subsize1.GetWidth(), subsize2.GetHeight() ); 
	imSize subsize4( subsize2.GetWidth(), subsize1.GetHeight() );
    
    imPoint origine1 = origine;
    imPoint origine2 = origine + subsize1;
    imPoint origine3 = imPoint( origine.x, origine.y + subsize1.GetHeight() );
    imPoint origine4 = imPoint( origine.x + subsize1.GetWidth(), origine.y );

    level++;
    row *= 2;
    column *= 2;
    
    if ( !SubRegister( origine1, subwindow, subsize1, level, row - 1, column - 1 ) )
    {
        return false;
    }


    // next level, we need to copy again the mask because the content might have been modified during the previous recursion
    m_opImTmp1 = cv::Mat( subsize2.GetHeight(), subsize2.GetWidth(), CV_8UC1 );
	if ( m_opImTmp1.empty() )
			return this->Terminate( ERR_MEMORY );
    buffer(cv::Rect(origine2.x - origine1.x, origine2.y - origine1.y, m_opImTmp1.cols, m_opImTmp1.rows)).copyTo(m_opImTmp1);
    m_opImTmp1.copyTo( m_im2(cv::Rect(origine2.x, origine2.y, m_opImTmp1.cols, m_opImTmp1.rows)) );
    ImageDestroy( m_opImTmp1 );
    if ( !SubRegister( origine2, subwindow, subsize2, level, row, column ) )
    {
        return false;
    }

    // next
    m_opImTmp1 = cv::Mat( subsize3.GetHeight(), subsize3.GetWidth(), CV_8UC1 );
	if ( m_opImTmp1.empty() )
			return this->Terminate( ERR_MEMORY );
    buffer(cv::Rect(0, origine3.y - origine1.y, m_opImTmp1.cols, m_opImTmp1.rows)).copyTo(m_opImTmp1);
    m_opImTmp1.copyTo( m_im2(cv::Rect(origine3.x, origine3.y, m_opImTmp1.cols, m_opImTmp1.rows)) );
    ImageDestroy( m_opImTmp1 );
    if ( !SubRegister( origine3, subwindow, subsize3, level, row, column - 1 ) )
    {
        return false;
    }


    // next
    m_opImTmp1 = cv::Mat( subsize4.GetHeight(), subsize4.GetWidth(), CV_8UC1 );
	if ( m_opImTmp1.empty() )
			return this->Terminate( ERR_MEMORY );
    buffer(cv::Rect(origine4.x - origine1.x, 0, m_opImTmp1.cols, m_opImTmp1.rows)).copyTo(m_opImTmp1);
    m_opImTmp1.copyTo( m_im2(cv::Rect(origine4.x, origine4.y, m_opImTmp1.cols, m_opImTmp1.rows)) );
    ImageDestroy( m_opImTmp1 );
    if ( !SubRegister( origine4, subwindow, subsize4, level, row - 1, column ) )
    {
        return false;
    }

	return true;
}
#endif




