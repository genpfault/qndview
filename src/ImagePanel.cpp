#include "ImagePanel.h"

#include <wx/dcbuffer.h>

#include <set>

using namespace std;


vector< wxRect > GetCoverage( const wxRect& viewport, const wxRect& canvas, const wxSize& gridSize )
{
    const wxRect clippedViewport( canvas.Intersect( viewport ) );

    vector< wxRect > coverage;
    const int top    = clippedViewport.GetTop()    / gridSize.y;
    const int bottom = clippedViewport.GetBottom() / gridSize.y;
    const int left   = clippedViewport.GetLeft()   / gridSize.x;
    const int right  = clippedViewport.GetRight()  / gridSize.x;
    for( int y = top; y <= bottom; ++y )
    {
        for( int x = left; x <= right; ++x )
        {
            const wxRect candidate( x * gridSize.x, y * gridSize.y, gridSize.x, gridSize.y );
            const wxRect clipped( canvas.Intersect( candidate ) );
            coverage.push_back( clipped );
        }
    }
    return coverage;
}


template< typename T >
T clamp( const T& val, const T& minVal, const T& maxVal )
{
    if( val < minVal )  return minVal;
    if( val > maxVal )  return maxVal;
    return val;
}


wxPoint ClampPosition( const wxRect& viewport, const wxRect& extent )
{
    const wxSize delta( viewport.GetSize() - extent.GetSize() ); 

    wxPoint newTopLeft( viewport.GetPosition() );

    if( delta.x < 0 )
    {
        // viewport smaller than extent
        int minX = extent.GetPosition().x;
        int maxX = ( extent.GetPosition().x + extent.GetSize().x ) - viewport.GetSize().x;
        newTopLeft.x = clamp( newTopLeft.x, minX, maxX );
    }
    else
    {
        // viewport larger than extent
        newTopLeft.x = extent.GetPosition().x - ( delta.x / 2 );
    }

    if( delta.y < 0 )
    {
        // viewport smaller than extent
        int minY = extent.GetPosition().y;
        int maxY = ( extent.GetPosition().y + extent.GetSize().y ) - viewport.GetSize().y;
        newTopLeft.y = clamp( newTopLeft.y, minY, maxY );
    }
    else
    {
        // viewport larger than extent
        newTopLeft.y = extent.GetPosition().y - ( delta.y / 2 );
    }

    return newTopLeft;
}



wxImagePanel::wxImagePanel( wxWindow* parent )
    : wxWindow( parent, wxID_ANY )
    , mBitmapCache( 512 )   // ~135MB for 512 256x256x4 byte tiles
    , mPosition( 0, 0 )
    , mScale( 1.0 )
    , mImageFactory( this )
    , mAnimationTimer( this )
{
    // for wxAutoBufferedPaintDC
    SetBackgroundStyle( wxBG_STYLE_PAINT );

    SetBackgroundColour( *wxBLACK );
        
    Bind( wxEVT_SIZE        , &wxImagePanel::OnSize           , this );
    Bind( wxEVT_PAINT       , &wxImagePanel::OnPaint          , this );
    Bind( wxEVT_KEY_DOWN    , &wxImagePanel::OnKeyDown        , this );
    Bind( wxEVT_KEY_UP      , &wxImagePanel::OnKeyUp          , this );
    Bind( wxEVT_LEFT_DOWN   , &wxImagePanel::OnButtonDown     , this );
    Bind( wxEVT_RIGHT_DOWN  , &wxImagePanel::OnButtonDown     , this );
    Bind( wxEVT_MIDDLE_DOWN , &wxImagePanel::OnButtonDown     , this );
    Bind( wxEVT_MOTION      , &wxImagePanel::OnMotion         , this );
    Bind( wxEVT_THREAD      , &wxImagePanel::OnThread         , this );
    Bind( wxEVT_TIMER       , &wxImagePanel::OnAnimationTimer , this, mAnimationTimer.GetId() );

    mStipple = wxBitmap( wxImage( "background.png" ) );
}


void wxImagePanel::OnSize( wxSizeEvent& event )
{
    mPosition = ClampPosition( mPosition );

    // invalidate entire panel since we need to redraw everything
    Refresh( false );

    // skip the event so sizers can do their thing
    event.Skip();
}


void wxImagePanel::OnButtonDown( wxMouseEvent& event )
{
    if( event.LeftDown() )
    {
        mLeftPositionStart = mPosition;
        mLeftMouseStart = event.GetPosition();
    }
}


void wxImagePanel::OnMotion( wxMouseEvent& event )
{
    if( event.LeftIsDown() && event.Dragging() )
    {
        wxPoint newPos( mLeftPositionStart - ( event.GetPosition() - mLeftMouseStart ) );
        if( newPos != mPosition )
        {
            ScrollToPosition( newPos );
        }
    }
}


void wxImagePanel::OnIdle( wxIdleEvent & )
{
    wxPoint newPos( mPosition );
    const int step = wxGetKeyState( WXK_CONTROL ) ? 10 : 1;
    if( wxGetKeyState( WXK_LEFT  ) )    newPos += step * wxPoint( -1,  0 );
    if( wxGetKeyState( WXK_RIGHT ) )    newPos += step * wxPoint(  1,  0 );
    if( wxGetKeyState( WXK_UP    ) )    newPos += step * wxPoint(  0, -1 );
    if( wxGetKeyState( WXK_DOWN  ) )    newPos += step * wxPoint(  0,  1 );

    if( newPos == mPosition )
    {
        Unbind( wxEVT_IDLE, &wxImagePanel::OnIdle, this );
        return;
    }

    ScrollToPosition( newPos );

    //event.RequestMore( true );
    //wxMilliSleep( 1 );
}


void wxImagePanel::OnKeyDown( wxKeyEvent& event )
{
    switch( event.GetKeyCode() )
    {
        case WXK_LEFT:
        case WXK_RIGHT:
        case WXK_UP:
        case WXK_DOWN:
            Bind( wxEVT_IDLE, &wxImagePanel::OnIdle, this );
            break;
        // zoom in
        case '=':
        case WXK_ADD:
        case WXK_NUMPAD_ADD:
            SetScale( mScale * 1.1 );
            break;
        // zoom out
        case '-':
        case WXK_SUBTRACT:
        case WXK_NUMPAD_SUBTRACT:
            SetScale( mScale / 1.1 );
            break;
        case ']':
            IncrementFrame( true );
            break;
        case '[':
            IncrementFrame( false );
            break;
        case 'P':
            Play( true );
            break;
        default:
            break;
    }
    event.Skip();
}


wxPoint wxImagePanel::ClampPosition( const wxPoint& newPos )
{
    if( NULL == mImage )
    {
        return newPos;
    }

    return ::ClampPosition
        (
        wxRect( newPos, GetSize() ),
        wxRect( wxPoint(0,0), mImage->GetSize() * mScale )
        );
}


void wxImagePanel::OnKeyUp( wxKeyEvent& event )
{
    if( NULL == mImage )
    {
        return;
    }

    switch( event.GetKeyCode() )
    {
        // fit image to window
        case 'X':
        case WXK_NUMPAD_MULTIPLY:
            {
                const double scaleWidth = ( GetSize().x / static_cast< double >( mImage->GetWidth() ) );
                const double scaleHeight = ( GetSize().y / static_cast< double >( mImage->GetHeight() ) );
                SetScale( min( scaleWidth, scaleHeight ) );
            }
            break;
        // zoom 1:1
        case 'Z':
        case WXK_NUMPAD_DIVIDE:
            SetScale( 1.0 );
            break;
        default:
            break;
    }
}


void wxImagePanel::ScrollToPosition( const wxPoint& newPos )
{
    const wxPoint clamped = ClampPosition( newPos );
    wxPoint delta( clamped - mPosition );
    ScrollWindow( -delta.x, -delta.y );
    mPosition = clamped;
}


void wxImagePanel::QueueRect( const wxRect& rect )
{
    // don't queue rects we have cached
    wxBitmapPtr bmpPtr;
    if( mBitmapCache.get( bmpPtr, rect ) )
        return;

    // don't queue rects we've already queued
    if( mQueuedRects.end() != mQueuedRects.find( rect ) )
        return;

    mQueuedRects.insert( rect );
    mImageFactory.AddRect( rect );
}


struct wxRectPointDistCmp
{
    wxPoint mPoint;
    wxRectPointDistCmp( const wxPoint& pt ) : mPoint( pt ) {}
    bool operator()( const wxRect& left, const wxRect& right ) const
    {
        const wxPoint leftCenter = left.GetPosition() + 0.5 * ( left.GetBottomRight() - left.GetTopLeft() );
        const wxPoint rightCenter = right.GetPosition() + 0.5 * ( right.GetBottomRight() - right.GetTopLeft() );
        const wxPoint leftDiff = ( leftCenter - mPoint );
        const wxPoint rightDiff = ( rightCenter - mPoint );
        const double leftDistSq = ( leftDiff.x * leftDiff.x + leftDiff.y * leftDiff.y );
        const double rightDistSq = ( rightDiff.x * rightDiff.x + rightDiff.y * rightDiff.y );
        return ( leftDistSq < rightDistSq );
    }
};

void wxImagePanel::OnPaint( wxPaintEvent& )
{
    wxPaintDC dc(this);
    //wxAutoBufferedPaintDC dc( this );

    if( NULL == mImage )
    {
        dc.Clear();
        return;
    }

    // only clear where we *won't* be drawing image tiles to help prevent flicker
    {
        const wxRect imageRect( -mPosition, mImage->GetSize() * mScale );
        const wxRect viewportRect( wxPoint( 0, 0 ), GetSize() );
        wxRegion region( viewportRect );
        region.Subtract( imageRect );
        dc.SetDeviceClippingRegion( region );
        dc.Clear();
        dc.DestroyClippingRegion();
    }

    dc.SetDeviceOrigin( -mPosition.x, -mPosition.y );

    const wxRect scaledRect( wxPoint( 0, 0 ), mImage->GetSize() * mScale );
    const wxSize gridSize( TILE_SIZE, TILE_SIZE );

    // get the set of tiles we need to draw
    set< wxRect, wxRectCmp > updateRects;
    for( wxRegionIterator upd( GetUpdateRegion() ); upd.HaveRects(); ++upd )
    {
        wxRect rect( upd.GetRect() );
        rect.SetPosition( rect.GetPosition() + mPosition );

        const vector< wxRect > ret = GetCoverage
            (
            rect,
            scaledRect,
            gridSize
            );
        updateRects.insert( ret.begin(), ret.end() );
    }

    dc.SetBrush( wxBrush( mStipple ) );
    dc.SetPen( *wxTRANSPARENT_PEN );

    for( const wxRect& rect : updateRects )
    {
        wxBitmapPtr bmpPtr;
        if( mBitmapCache.get( bmpPtr, rect ) )
        {
            dc.DrawRectangle( rect );
            dc.DrawBitmap( *bmpPtr, rect.GetPosition() );
        }
    }

    for( const wxRect& rect : updateRects )
        QueueRect( rect );

    const wxRect viewport( wxRect( mPosition, GetSize() ).Inflate( GetSize() * 0.1 ) );
    for( const wxRect& rect : GetCoverage( viewport, scaledRect, gridSize ) )
        QueueRect( rect );

    const wxPoint center = viewport.GetPosition() + 0.5 * ( viewport.GetBottomRight() - viewport.GetTopLeft() );
    mImageFactory.Sort( wxRectPointDistCmp( center ) );
}


void wxImagePanel::SetImages( const AnimationFrames& newImages )
{
    if( newImages.empty() )
        return;

    mFrames = newImages;
    mCurFrame = 0;
    SetImage( mFrames[ mCurFrame ].mImage );

    if( mFrames.size() > 1 )
    {
        Play( false );
    }
}

void wxImagePanel::SetImage( wxSharedPtr< wxImage > newImage )
{
    mImage = newImage;
    mQueuedRects.clear();
    mBitmapCache.clear();
    mImageFactory.SetImage( mImage, mScale );
    mPosition = ClampPosition( mPosition );
    Refresh( false );
}


void wxImagePanel::SetScale( const double newScale )
{
    mScale = newScale;
    mBitmapCache.clear();
    mPosition = ClampPosition( mPosition );

    // invalidate entire panel since we need to redraw everything
    Refresh( false );

    if( NULL == mImage )
    {
        return;
    }

    mQueuedRects.clear();
    mImageFactory.SetScale( mScale );
}


void wxImagePanel::OnThread( wxThreadEvent& )
{
    wxRect rect;
    wxSharedPtr< wxImage > image;
    while( mImageFactory.GetImage( rect, image ) )
    {
        wxBitmapPtr bmp( new wxBitmap( *image ) );
        mBitmapCache.insert( rect, bmp );

        mQueuedRects.erase( rect );

        const wxRect target( rect.GetPosition() - mPosition, rect.GetSize() );
        const wxRect clipped( GetRect().Intersect( target ) );
        RefreshRect( clipped, false );
    }
}

void wxImagePanel::Play( bool toggle )
{
    if( mFrames.size() <= 1 )
    {
        return;
    }

    if( toggle && mAnimationTimer.IsRunning() )
    {
        mAnimationTimer.Stop();
    }
    else
    {
        if( mFrames[ mCurFrame ].mDelay >= 0 )
        {
            mAnimationTimer.Stop();
            mAnimationTimer.StartOnce( mFrames[ mCurFrame ].mDelay );
        }
    }
}

void wxImagePanel::IncrementFrame( bool forward )
{
    if( mFrames.size() <= 1 )
    {
        return;
    }

    if( forward )
    {
        mCurFrame++;
        if( mCurFrame >= mFrames.size() )
            mCurFrame = 0;
    }
    else
    {
        if( mCurFrame == 0 )
            mCurFrame = mFrames.size() - 1;
        else
            mCurFrame--;
    }

    SetImage( mFrames[ mCurFrame ].mImage );
}

void wxImagePanel::OnAnimationTimer( wxTimerEvent& WXUNUSED( event ) )
{
    IncrementFrame( true );
    Play( false );
}