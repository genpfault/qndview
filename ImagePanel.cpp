#include "ImagePanel.h"

#include <set>

using namespace std;


wxBitmapPtr ToBitmap( const LinearImage& image )
{
    vector< unsigned char > color, alpha;
    image.GetSrgb( color, alpha );
    return wxBitmapPtr( new wxBitmap( 
        alpha.empty()
        ? wxImage( image.GetWidth(), image.GetHeight(), &color[0], true )
        : wxImage( image.GetWidth(), image.GetHeight(), &color[0], &alpha[0], true )
        ) );
}


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


wxBitmapPtr GetScaledSubrect( const LinearImage& src, auto_ptr< Resampler > resamplers[ 4 ], const wxRect& rect )
{
    LinearImage dst( rect.GetWidth(), rect.GetHeight(), src.GetNumChannels() == 4 ? true : false, NULL );

    for( size_t c = 0; c < src.GetNumChannels(); ++c )
    {
        resamplers[ c ]->StartResample
            (
            rect.x, rect.y,
            rect.GetWidth(), rect.GetHeight()
            );
    }

    size_t dstY = 0;
    for( size_t y = 0; y < src.GetHeight(); ++y )
    {
        for( size_t c = 0; c < src.GetNumChannels(); ++c )
        {
            resamplers[ c ]->PutLine( src.GetRow( c, y ) );
        }

        while( true )
        {
            bool missedLine = false;
            for( size_t c = 0; c < src.GetNumChannels(); ++c )
            {
                const float* line = resamplers[ c ]->GetLine();
                if( NULL == line )
                {
                    missedLine = true;
                    break;
                }

                copy( line, line + dst.GetWidth(), dst.GetRow( c, dstY ) );
            }

            if( missedLine )
            {
                break;
            }

            dstY++;
        }
    }

    return ToBitmap( dst );
}


wxImagePanel::wxImagePanel( wxWindow* parent )
    : wxWindow( parent, wxID_ANY )
    , mPosition( 0, 0 )
    , mScale( 1.0 )
{
    // for wxAutoBufferedPaintDC
    SetBackgroundStyle( wxBG_STYLE_PAINT );

    SetBackgroundColour( *wxBLACK );
        
    Bind( wxEVT_SIZE        , &wxImagePanel::OnSize         , this );
    Bind( wxEVT_PAINT       , &wxImagePanel::OnPaint        , this );
    Bind( wxEVT_KEY_DOWN    , &wxImagePanel::OnKeyDown      , this );
    Bind( wxEVT_KEY_UP      , &wxImagePanel::OnKeyUp        , this );
    Bind( wxEVT_LEFT_DOWN   , &wxImagePanel::OnButtonDown   , this );
    Bind( wxEVT_RIGHT_DOWN  , &wxImagePanel::OnButtonDown   , this );
    Bind( wxEVT_MIDDLE_DOWN , &wxImagePanel::OnButtonDown   , this );
    Bind( wxEVT_MOTION      , &wxImagePanel::OnMotion       , this );
}


void wxImagePanel::OnSize( wxSizeEvent& event )
{
    mPosition = ClampPosition( mPosition );

    // invalidate entire panel since we need to redraw everything
    Refresh( true );

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


void wxImagePanel::OnIdle( wxIdleEvent& event )
{
    wxPoint newPos( mPosition );
    const int step = 1;
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
        default:
            break;
    }
}


wxPoint wxImagePanel::ClampPosition( const wxPoint& newPos )
{
    if( NULL == mImage )
    {
        return newPos;
    }

    const wxSize scaledSize( mImage->GetWidth() * mScale, mImage->GetHeight() * mScale );
    return ::ClampPosition
        (
        wxRect( newPos, GetSize() ),
        wxRect( wxPoint(0,0), scaledSize )
        );
}


void wxImagePanel::OnKeyUp( wxKeyEvent& event )
{
    switch( event.GetKeyCode() )
    {
        case WXK_ADD:
        case WXK_NUMPAD_ADD:
            SetScale( mScale * 1.1 );
            break;
        case WXK_SUBTRACT:
        case WXK_NUMPAD_SUBTRACT:
            SetScale( mScale / 1.1 );
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


void wxImagePanel::OnPaint( wxPaintEvent& event )
{
    wxPaintDC dc(this);
    //wxAutoBufferedPaintDC dc( this );

    dc.SetDeviceOrigin( -mPosition.x, -mPosition.y );

    dc.Clear();

    if( NULL == mImage )
    {
        return;
    }

    const wxSize scaledSize( mImage->GetWidth() * mScale, mImage->GetHeight() * mScale );
    const wxRect scaledRect( wxPoint( 0, 0 ), scaledSize );
    const wxSize gridSize( TILE_SIZE, TILE_SIZE );

    // get the set of tiles we need to draw
    set< wxRect, wxRectCmp > covered;
    for( wxRegionIterator upd( GetUpdateRegion() ); upd.HaveRects(); ++upd )
    {
        wxRect rect( upd.GetRect() );
        rect.SetPosition( rect.GetPosition() + mPosition );

        const vector< wxRect > ret = GetCoverage( rect, scaledRect, gridSize );
        covered.insert( ret.begin(), ret.end() );
    }
        
    for( const wxRect& rect : covered )
    {
        map< wxRect, wxBitmapPtr >::iterator it = mBitmapCache.find( rect );
        if( mBitmapCache.end() == it )
        {
            it = mBitmapCache.insert( make_pair( rect, GetScaledSubrect( *mImage, mResamplers, rect ) ) ).first;
        }

        dc.DrawBitmap( *(it->second), rect.GetPosition() );
    }
}


void wxImagePanel::SetImage( const LinearImage* newImage )
{
    mImage = newImage;
    SetScale( mScale );
    mPosition = ClampPosition( mPosition );
}


void wxImagePanel::SetScale( const double newScale )
{
    mScale = newScale;
    mBitmapCache.clear();
    mPosition = ClampPosition( mPosition );

    // invalidate entire panel since we need to redraw everything
    Refresh( true );

    if( NULL == mImage )
    {
        return;
    }

    // regenerate resamplers
    mContribLists.reset( new Resampler::ContribLists
        (
        mImage->GetWidth(), mImage->GetHeight(),
        mImage->GetWidth() * mScale, mImage->GetHeight() * mScale
        ) );
    for( size_t i = 0; i < 4; ++i )
    {
        mResamplers[ i ].reset( new Resampler( *mContribLists ) );
    }
}