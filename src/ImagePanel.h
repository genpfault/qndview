#ifndef IMAGEPANEL_H
#define IMAGEPANEL_H

#include <wx/wx.h>

#include <memory>
#include <map>
#include <set>

#include "ScaledImageFactory.h"
#include "LruCache.h"


struct AnimationFrame
{
    wxSharedPtr< wxImage > mImage;

    // in milliseconds
    int mDelay;
};
typedef std::vector< AnimationFrame > AnimationFrames;

class wxImagePanel : public wxWindow
{
public:
    wxImagePanel( wxWindow* parent );

    void SetImages( const AnimationFrames& newImages );
    void SetScale( const double newScale );

private:
    void SetImage( wxSharedPtr< wxImage > newImage );

    void OnSize( wxSizeEvent& event );
    void OnButtonDown( wxMouseEvent& event );
    void OnMotion( wxMouseEvent& event );
    void OnIdle( wxIdleEvent& event );
    void OnKeyDown( wxKeyEvent& event );
    void OnKeyUp( wxKeyEvent& event );
    void OnPaint( wxPaintEvent& event );
    void OnThread( wxThreadEvent& event );
    void OnAnimationTimer( wxTimerEvent& event );

    wxPoint ClampPosition( const wxPoint& newPos );
    void ScrollToPosition( const wxPoint& newPos );
    void QueueRect( const ExtRect& rect );

    void Play( bool pause );
    void IncrementFrame( bool forward );

    static const size_t TILE_SIZE = 256;   // pixels

    size_t mCurFrame;
    AnimationFrames mFrames;
    wxSharedPtr< wxImage > mImage;

    typedef wxSharedPtr< wxBitmap > wxBitmapPtr;
    LruCache< ExtRect, wxBitmapPtr > mBitmapCache;

    // position of the top-left of the viewport
    wxPoint mPosition;
    double mScale;

    wxPoint mLeftPositionStart;
    wxPoint mLeftMouseStart;

    ScaledImageFactory mImageFactory;
    std::set< ExtRect > mQueuedRects;

    wxTimer mAnimationTimer;
};

#endif
