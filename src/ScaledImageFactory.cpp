#include "ScaledImageFactory.h"

#include <memory>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize.h>

using namespace std;


void GetScaledSubrect( wxImage& dst, const wxImage& src, const double scale, const wxPoint& pos )
{
#if 0
    const size_t srcW = static_cast< size_t >( src.GetWidth() );
    const size_t dstW = static_cast< size_t >( dst.GetWidth() );
    const size_t dstH = static_cast< size_t >( dst.GetHeight() );

    const float scaleInv = 1.0f / scale;

    // color
    {
        const unsigned char* srcData = src.GetData();
        unsigned char* dstData = dst.GetData();

        for( size_t dstY = 0; dstY < dstH; ++dstY )
        {
            unsigned char* dstRow = &dstData[ dstY * dstW * 3 ];
    
            const size_t srcY( ( dstY + pos.y ) * scaleInv );
            const unsigned char* srcRow = &srcData[ srcY * srcW * 3 ];

            for( size_t dstX = 0; dstX < dstW; ++dstX )
            {
                const size_t srcX( ( dstX + pos.x ) * scaleInv );
                const unsigned char* srcPx = &srcRow[ srcX * 3 ];
                dstRow[ dstX * 3 + 0 ] = srcPx[ 0 ];
                dstRow[ dstX * 3 + 1 ] = srcPx[ 1 ];
                dstRow[ dstX * 3 + 2 ] = srcPx[ 2 ];
            }
        }
    }

    if( !src.HasAlpha() )
        return;

    // alpha
    {
        const unsigned char* srcData = src.GetAlpha();
        unsigned char* dstData = dst.GetAlpha();

        for( size_t dstY = 0; dstY < dstH; ++dstY )
        {
            unsigned char* dstRow = &dstData[ dstY * dstW ];
    
            const size_t srcY( ( dstY + pos.y ) * scaleInv );
            const unsigned char* srcRow = &srcData[ srcY * srcW ];

            for( size_t dstX = 0; dstX < dstW; ++dstX )
            {
                const size_t srcX( ( dstX + pos.x ) * scaleInv );
                const unsigned char* srcPx = &srcRow[ srcX ];
                dstRow[ dstX + 0 ] = srcPx[ 0 ];
            }
        }
    }

    return;
#endif

    const stbir_filter filter = STBIR_FILTER_TRIANGLE;
    const stbir_edge edge = STBIR_EDGE_CLAMP;
    const stbir_colorspace colorspace = STBIR_COLORSPACE_SRGB;

    stbir_resize_subpixel
        (
        src.GetData(), src.GetWidth(), src.GetHeight(), 0,
        dst.GetData(), dst.GetWidth(), dst.GetHeight(), 0,
        STBIR_TYPE_UINT8,
        3,
        0,
        STBIR_ALPHA_CHANNEL_NONE,
        edge, edge,
        filter, filter,
        colorspace,
        NULL,
        scale, scale,
        static_cast< float >( pos.x ), static_cast< float >( pos.y )
        );

    if( !src.HasAlpha() )
        return;

    stbir_resize_subpixel
        (
        src.GetAlpha(), src.GetWidth(), src.GetHeight(), 0,
        dst.GetAlpha(), dst.GetWidth(), dst.GetHeight(), 0,
        STBIR_TYPE_UINT8,
        1,
        0,
        STBIR_FLAG_ALPHA_PREMULTIPLIED,
        edge, edge,
        filter, filter,
        colorspace,
        NULL,
        scale, scale,
        static_cast< float >( pos.x ), static_cast< float >( pos.y )
        );
}


// threadland
wxThread::ExitCode ScaledImageFactory::Entry()
{
    JobItem job;
    while( wxSORTABLEMSGQUEUE_NO_ERROR == mJobPool.Receive( job ) )
    {
        if( NULL == job.second.mImage || wxThread::This()->TestDestroy() )
            break;

        const wxRect& rect = job.first;
        Context& ctx = job.second;

        ResultItem result;
        result.mGeneration = ctx.mGeneration;
        result.mRect = rect;

        result.mImage = new wxImage( rect.GetWidth(), rect.GetHeight(), false );
        if( ctx.mImage->HasAlpha() )
        {
            result.mImage->SetAlpha( NULL );
        }

        GetScaledSubrect
            (
            *result.mImage,
            *ctx.mImage,
            ctx.mScale,
            rect.GetPosition()
            );
        mResultQueue.Post( result );

        wxQueueEvent( mEventSink, new wxThreadEvent( wxEVT_THREAD, mEventId ) );
    }

    return static_cast< wxThread::ExitCode >( 0 );
}


ScaledImageFactory::ScaledImageFactory( wxEvtHandler* eventSink, int id )
    : mEventSink( eventSink ), mEventId( id )
{
    size_t numThreads = wxThread::GetCPUCount();
    if( numThreads <= 0 )   numThreads = 1;
    if( numThreads > 1 )    numThreads--;

    for( size_t i = 0; i < numThreads; ++i )
    {
        CreateThread();
    }

    for( wxThread*& thread : GetThreads() )
    {
        if( NULL == thread )
            continue;

        if( thread->Run() != wxTHREAD_NO_ERROR )
        {
            delete thread;
            thread = NULL;
        }
    }

    mCurrentCtx.mGeneration = 0;
}

ScaledImageFactory::~ScaledImageFactory()
{
    // clear job queue and send down "kill" jobs
    mJobPool.Clear();
    for( size_t i = 0; i < GetThreads().size(); ++i )
    {
        mJobPool.Post( JobItem( wxRect(), Context() ) );
    }

    for( wxThread* thread : GetThreads() )
    {
        if( NULL == thread )
            continue;

        thread->Wait();
    }
}

void ScaledImageFactory::SetImage( wxImagePtr& newImage, double newScale )
{
    if( NULL == newImage )
        throw std::runtime_error( "Image not set!" );

    mCurrentCtx.mGeneration++;
    mCurrentCtx.mImage = newImage;
    mCurrentCtx.mScale = newScale;
    mJobPool.Clear();
}

bool ScaledImageFactory::AddRect( const wxRect& rect )
{
    if( NULL == mCurrentCtx.mImage )
        throw std::runtime_error( "Image not set!" );

    return( wxSORTABLEMSGQUEUE_NO_ERROR == mJobPool.Post( JobItem( rect, mCurrentCtx ) ) );
}

bool ScaledImageFactory::GetImage( wxRect& rect, wxImagePtr& image )
{
    ResultItem item;
    wxMessageQueueError err;
    while( true )
    {
        err = mResultQueue.ReceiveTimeout( 0, item );
        if( wxMSGQUEUE_TIMEOUT == err )
            return false;
        if( wxMSGQUEUE_MISC_ERROR == err )
            throw std::runtime_error( "ResultQueue misc error!" );
        if( item.mGeneration != mCurrentCtx.mGeneration )
            continue;
        break;
    }

    rect = item.mRect;
    image = item.mImage;
    return true;
}

void ScaledImageFactory::ClearQueue()
{
    mJobPool.Clear();
}
