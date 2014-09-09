#ifndef IMAGEPANEL_H
#define IMAGEPANEL_H

#include <wx/wx.h>
#include <wx/msgqueue.h>

#include <memory>
#include <map>
#include <set>

#include "LinearImage.h"
#include "ScaledImageFactory.h"


class wxImagePanel : public wxWindow
{
public:
    wxImagePanel( wxWindow* parent );

    void SetImage( wxSharedPtr< LinearImage > newImage );
    void SetScale( const double newScale );

private:
    void OnSize( wxSizeEvent& event );
    void OnButtonDown( wxMouseEvent& event );
    void OnMotion( wxMouseEvent& event );
    void OnIdle( wxIdleEvent& event );
    void OnKeyDown( wxKeyEvent& event );
    void OnKeyUp( wxKeyEvent& event );
    void OnPaint( wxPaintEvent& event );
    void OnThread( wxThreadEvent& event );
    wxPoint ClampPosition( const wxPoint& newPos );
    void ScrollToPosition( const wxPoint& newPos );

    static const size_t TILE_SIZE = 512;   // pixels

    wxSharedPtr< LinearImage > mImage;

    // (ab)use std::pair<>'s operator<() to compare wxRects
    struct wxRectCmp
    { 
        bool operator()( const wxRect& left, const wxRect& right ) const
        {
            const std::pair< int, int >  leftPair(  left.GetTop(),  left.GetLeft() );
            const std::pair< int, int > rightPair( right.GetTop(), right.GetLeft() );
            return ( leftPair < rightPair );
        }
    };
    typedef wxSharedPtr< wxBitmap > wxBitmapPtr;
    std::map< wxRect, wxBitmapPtr, wxRectCmp > mBitmapCache;

    // position of the top-left of the viewport
    wxPoint mPosition;
    double mScale;

    wxPoint mLeftPositionStart;
    wxPoint mLeftMouseStart;

    ScaledImageFactory mImageFactory;
    std::set< wxRect, wxRectCmp > mQueuedRects;
};

#endif