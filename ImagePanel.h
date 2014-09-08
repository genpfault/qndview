#ifndef IMAGEPANEL_H
#define IMAGEPANEL_H

#include <wx/wx.h>
#include <memory>
#include <map>

#include "imageresampler/resampler.h"
#include "LinearImage.h"

typedef wxSharedPtr< wxBitmap > wxBitmapPtr;

class wxImagePanel : public wxWindow
{
public:
    wxImagePanel( wxWindow* parent );

    void SetImage( const LinearImage* newImage );
    void SetScale( const double newScale );

private:
    void OnSize( wxSizeEvent& event );
    void OnButtonDown( wxMouseEvent& event );
    void OnMotion( wxMouseEvent& event );
    void OnIdle( wxIdleEvent& event );
    void OnKeyDown( wxKeyEvent& event );
    void OnKeyUp( wxKeyEvent& event );
    void OnPaint( wxPaintEvent& event );
    wxPoint ClampPosition( const wxPoint& newPos );
    void ScrollToPosition( const wxPoint& newPos );

    static const size_t TILE_SIZE = 512;   // pixels

    LinearImage const* mImage;

    // resampler state
    std::auto_ptr< Resampler::ContribLists > mContribLists;
    std::auto_ptr< Resampler > mResamplers[ 4 ];

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
    std::map< wxRect, wxBitmapPtr, wxRectCmp > mBitmapCache;

    // position of the top-left of the viewport
    wxPoint mPosition;
    double mScale;

    wxPoint mLeftPositionStart;
    wxPoint mLeftMouseStart;
};

#endif