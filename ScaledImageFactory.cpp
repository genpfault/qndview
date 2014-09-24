#include "ScaledImageFactory.h"

#include <memory>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize.h>

using namespace std;


void GetScaledSubrect( wxImage& dst, const wxImage& src, const double scale, const wxPoint& pos )
{
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


class WorkerThread : public wxThread
{
public:
    WorkerThread
        (
        wxEvtHandler* eventSink,
        int id,
        ScaledImageFactory::JobPoolType& mJobPool,
        ScaledImageFactory::ResultQueueType& mResultQueue
        )
        : wxThread( wxTHREAD_JOINABLE )
        , mEventSink( eventSink ), mEventId( id ), mJobPool( mJobPool ), mResultQueue( mResultQueue )
    { }

    virtual ExitCode Entry()
    {
        ScaledImageFactory::JobItem job;
        while( wxSORTABLEMSGQUEUE_NO_ERROR == mJobPool.Receive( job ) )
        {
            if( NULL == job.second.mImage || TestDestroy() )
                break;

            const wxRect& rect = job.first;
            ScaledImageFactory::Context& ctx = job.second;

            ScaledImageFactory::ResultItem result;
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

private:
    wxEvtHandler* mEventSink;
    int mEventId;
    ScaledImageFactory::JobPoolType& mJobPool;
    ScaledImageFactory::ResultQueueType& mResultQueue;
};


ScaledImageFactory::ScaledImageFactory( wxEvtHandler* eventSink, int id )
{
    size_t numThreads = wxThread::GetCPUCount();
    if( numThreads <= 0 )   numThreads = 1;
    if( numThreads > 1 )    numThreads--;

    for( size_t i = 0; i < numThreads; ++i )
    {
        mThreads.push_back( new WorkerThread( eventSink, id, mJobPool, mResultQueue ) );
    }

    for( wxThread*& thread : mThreads )
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
    for( size_t i = 0; i < mThreads.size(); ++i )
    {
        mJobPool.Post( JobItem( wxRect(), Context() ) );
    }

    for( wxThread* thread : mThreads )
    {
        if( NULL == thread )
            continue;

        thread->Wait();
        delete thread;
    }
}

void ScaledImageFactory::SetImage( wxImagePtr& newImage, double scale )
{
    mCurrentCtx.mGeneration++;
    mJobPool.Clear();
    mCurrentCtx.mImage = newImage;
    SetScale( scale );
}

void ScaledImageFactory::SetScale( double newScale )
{
    if( NULL == mCurrentCtx.mImage )
        throw std::runtime_error( "Image not set!" );

    mCurrentCtx.mGeneration++;
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
