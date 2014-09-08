#include <wx/wx.h>
#include <wx/dcbuffer.h>

#include <string>
#include <set>

#include "imageresampler/resampler.h"

using namespace std;


class LinearImage
{
public:
    LinearImage( const size_t width, const size_t height, const bool hasAlpha, const unsigned char* data, const float gamma = 2.2f )
        : mWidth( width ), mHeight( height )
    {
        InitTables( gamma );

        // allocate channels
        mChannels.resize( hasAlpha ? 4 : 3 );
        for( Channel& channel : mChannels )
        {
            channel.resize( mWidth * mHeight );
        }

        if( NULL == data )
        {
            return;
        }

        // convert to linear color
        const size_t srcPitch = mWidth * 3;
        const unsigned char* srcBytes = data;
        for( size_t y = 0; y < (size_t)mHeight; ++y )
        {
            const size_t dstRow = y * mWidth;
            const unsigned char* srcRow = &srcBytes[ y * srcPitch ];
            for( size_t x = 0; x < (size_t)mWidth; ++x )
            {
                for( size_t c = 0; c < 3; ++c )
                {
                    mChannels[ c ][ dstRow + x ] = mSrgbToLinear[ srcRow[ x * 3 + c ] ];
                }
                if( hasAlpha )
                {
                    mChannels[ 3 ][ dstRow + x ] = srcRow[ x * 3 + 3 ] * ( 1.0f / 255.0f );
                }
            }
        }
    }

    void GetSrgb( vector< unsigned char >& color, vector< unsigned char >& alpha ) const
    {
        color.resize( mWidth * mHeight * 3 );
        for( size_t i = 0; i < mWidth * mHeight; ++i )
        {
            for( size_t c = 0; c < 3; ++c )
            {
                int j = static_cast< int >( mLinearToSrgb.size() * mChannels[ c ][ i ] + 0.5f );
                if( j < 0 )                             j = 0;
                // TODO: figure out signed issues
                if( j >= (int)mLinearToSrgb.size() )    j = (int)( mLinearToSrgb.size() - 1 );
                color[ i * 3 + c ] = mLinearToSrgb[ j ];
            }
        }

        alpha.resize( 0 );
        if( mChannels.size() == 4 )
        {
            alpha.resize( mWidth * mHeight );
            for( size_t i = 0; i < mWidth * mHeight; ++i )
            {
                int j = static_cast< int >( 255.0f * mChannels[ 3 ][ i ] + 0.5f );
                if( j < 0 )     j = 0;
                if( j >= 255 )  j = 255;
                alpha[ i ] = static_cast< unsigned char >( j );
            }
        }
    }

    const float* GetRow( const size_t channel, const size_t row ) const
    {
        if( channel > mChannels.size() || row >= mHeight )
            return NULL;
        return &mChannels[ channel ][ mWidth * row ];
    }

    float* GetRow( const size_t channel, const size_t row )
    {
        return const_cast< float* >( static_cast< const LinearImage& >( *this ).GetRow( channel, row ) );
    }

    const size_t GetWidth()     const { return mWidth;  }
    const size_t GetHeight()    const { return mHeight; }
    const size_t GetNumChannels() const { return mChannels.size(); }

private:
    void InitTables( const float gamma )
    {
        mSrgbToLinear.resize( 256 );
        for( size_t i = 0; i < mSrgbToLinear.size(); ++i )
        {
            mSrgbToLinear[ i ] = pow( i / 255.0f, gamma );
        }

        mLinearToSrgb.resize( 4096 );
        const float invSize = 1.0f / mLinearToSrgb.size();
        const float invGamma = 1.0f / gamma;
        for( size_t i = 0; i < mLinearToSrgb.size(); ++i )
        {
            int k = static_cast< int >( 255.0f * pow( i * invSize, invGamma ) + 0.5f );
            if( k < 0 )     k = 0;
            if( k > 255 )   k = 255;
            mLinearToSrgb[ i ] = static_cast< unsigned char >( k );
        }
    }

    size_t mWidth, mHeight;
    typedef vector< float > Channel;
    typedef vector< Channel > Channels;
    Channels mChannels;
    vector< float > mSrgbToLinear;
    vector< unsigned char > mLinearToSrgb;
};


typedef wxSharedPtr< wxBitmap > wxBitmapPtr;
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


// (ab)use std::pair<>'s operator<() to compare wxRects
struct wxRectCmp
{ 
    bool operator()( const wxRect& left, const wxRect& right ) const
    {
        const pair< int, int >  leftPair(  left.GetTop(),  left.GetLeft() );
        const pair< int, int > rightPair( right.GetTop(), right.GetLeft() );
        return ( leftPair < rightPair );
    }
};


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


class wxImagePanel : public wxWindow
{
    static const size_t TILE_SIZE = 512;   // pixels

public:
    wxImagePanel( wxWindow* parent )
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

    void OnSize( wxSizeEvent& event )
    {
        mPosition = ClampPosition( mPosition );

        // invalidate entire panel since we need to redraw everything
        Refresh( true );

        // skip the event so sizers can do their thing
        event.Skip();
    }

    void OnButtonDown( wxMouseEvent& event )
    {
        if( event.LeftDown() )
        {
            mLeftPositionStart = mPosition;
            mLeftMouseStart = event.GetPosition();
        }
    }

    void OnMotion( wxMouseEvent& event )
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

    void OnIdle( wxIdleEvent& event )
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

    void OnKeyDown( wxKeyEvent& event )
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

    wxPoint ClampPosition( const wxPoint& newPos )
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

    void OnKeyUp( wxKeyEvent& event )
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

    void ScrollToPosition( const wxPoint& newPos )
    {
        const wxPoint clamped = ClampPosition( newPos );
        wxPoint delta( clamped - mPosition );
        ScrollWindow( -delta.x, -delta.y );
        mPosition = clamped;
    }

    void OnPaint( wxPaintEvent& event )
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

    void SetImage( const LinearImage* newImage )
    {
        mImage = newImage;
        SetScale( mScale );
        mPosition = ClampPosition( mPosition );
    }

    void SetScale( const double newScale )
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

private:
    LinearImage const* mImage;

    // resampler state
    auto_ptr< Resampler::ContribLists > mContribLists;
    auto_ptr< Resampler > mResamplers[ 4 ];

    map< wxRect, wxBitmapPtr, wxRectCmp > mBitmapCache;

    // position of the top-left of the viewport
    wxPoint mPosition;
    double mScale;

    wxPoint mLeftPositionStart;
    wxPoint mLeftMouseStart;
};


// IDs for the controls and the menu commands
enum
{
    // menu items
    Minimal_Quit = wxID_EXIT,

    // it is important for the id corresponding to the "About" command to have
    // this standard value as otherwise it won't be handled properly under Mac
    // (where it is special and put into the "Apple" menu)
    Minimal_About = wxID_ABOUT
};

// Define a new frame type: this is going to be our main frame
class MyFrame : public wxFrame
{
public:
    MyFrame(const wxString& title)
        : wxFrame( NULL, wxID_ANY, title )
        , mImagePanel( new wxImagePanel( this ) )
    {
        // create a menu bar
        wxMenu *fileMenu = new wxMenu;

        // the "About" item should be in the help menu
        wxMenu *helpMenu = new wxMenu;
        helpMenu->Append(Minimal_About, "&About\tF1", "Show about dialog");

        fileMenu->Append(Minimal_Quit, "E&xit\tAlt-X", "Quit this program");

        // now append the freshly created menu to the menu bar...
        wxMenuBar *menuBar = new wxMenuBar();
        menuBar->Append(fileMenu, "&File");
        menuBar->Append(helpMenu, "&Help");

        // ... and attach this menu bar to the frame
        SetMenuBar(menuBar);

        // create a status bar just for fun (by default with 1 pane only)
        CreateStatusBar(2);
        SetStatusText("Welcome to wxWidgets!");

        string fileName( "test.png" );
        bool success = false;
        {
            // bug workaround
            // http://trac.wxwidgets.org/ticket/15331
            wxLogNull logNo;
            success = mImage.LoadFile( fileName );
        }
        //mLinearImage

        mLinearImage.reset( new LinearImage
            (
            mImage.GetWidth(),
            mImage.GetHeight(),
            mImage.HasAlpha(),
            mImage.GetData()
            ) );

        mImagePanel->SetImage( mLinearImage.get() );
    }

    // event handlers (these functions should _not_ be virtual)
    void OnQuit(wxCommandEvent& WXUNUSED(event))
    {
        // true is to force the frame to close
        Close(true);
    }

    void OnAbout(wxCommandEvent& WXUNUSED(event))
    {
        wxMessageBox
            (
            wxString::Format
                (
                "Welcome to %s!\n"
                "\n"
                "This is the minimal wxWidgets sample\n"
                "running under %s.",
                wxVERSION_STRING,
                wxGetOsDescription()
                ),
            "About wxWidgets minimal sample",
            wxOK | wxICON_INFORMATION,
            this
            );
    }

private:
    wxImagePanel* mImagePanel;
    wxImage mImage;
    auto_ptr< LinearImage > mLinearImage;

    // any class wishing to process wxWidgets events must use this macro
    wxDECLARE_EVENT_TABLE();
};

// the event tables connect the wxWidgets events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(Minimal_Quit,  MyFrame::OnQuit)
    EVT_MENU(Minimal_About, MyFrame::OnAbout)
wxEND_EVENT_TABLE()


// Define a new application type, each program should derive a class from wxApp
class MyApp : public wxApp
{
public:
    // this one is called on application startup and is a good place for the app
    // initialization (doing it here and not in the ctor allows to have an error
    // return: if OnInit() returns false, the application terminates)
    // 'Main program' equivalent: the program execution "starts" here
    virtual bool OnInit()
    {
        // call the base class initialization method, currently it only parses a
        // few common command-line options but it could be do more in the future
        if ( !wxApp::OnInit() )
            return false;

        // handle ALL the images!
        wxInitAllImageHandlers();

        // create the main application window
        MyFrame *frame = new MyFrame("Minimal wxWidgets App");

        // and show it (the frames, unlike simple controls, are not shown when
        // created initially)
        frame->Show(true);

        // success: wxApp::OnRun() will be called which will enter the main message
        // loop and the application will run. If we returned false here, the
        // application would exit immediately.
        return true;
    }
};

// Create a new application object: this macro will allow wxWidgets to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also implements the accessor function
// wxGetApp() which will return the reference of the right type (i.e. MyApp and
// not wxApp)
IMPLEMENT_APP(MyApp)