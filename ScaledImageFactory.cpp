#include "ScaledImageFactory.h"

#include <memory>

#include "LinearImage.h"

using namespace std;


void GetScaledSubrect( LinearImage& dst, const LinearImage& src, auto_ptr< Resampler > resamplers[ 4 ], const wxPoint& pos )
{
    for( size_t c = 0; c < src.GetNumChannels(); ++c )
    {
        resamplers[ c ]->StartResample
            (
            pos.x, pos.y,
            dst.GetWidth(), dst.GetHeight()
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
                if( !resamplers[ c ]->GetLine( dst.GetRow( c, dstY ) ) )
                {
                    missedLine = true;
                }
            }

            if( missedLine )
            {
                break;
            }

            dstY++;
        }
    }
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
        wxSharedPtr< Resampler::ContribLists > curContribLists;
        auto_ptr< Resampler > resamplers[ 4 ];
		auto_ptr< LinearImage > dstImage;

        ScaledImageFactory::JobItem job;
        while( wxSORTABLEMSGQUEUE_NO_ERROR == mJobPool.Receive( job ) )
        {
            if( NULL == job.second.mImage || TestDestroy() )
                break;

            const wxRect& rect = job.first;
            ScaledImageFactory::Context& ctx = job.second;

            if( curContribLists != ctx.mContribLists )
            {
                curContribLists = ctx.mContribLists;
                for( size_t i = 0; i < 4; ++i )
                {
                    resamplers[ i ].reset( new Resampler( *curContribLists ) );
                }
            }

            // regenerate destination image if needed
            if( NULL == dstImage.get() || 
                dstImage->GetWidth() != static_cast< unsigned int >( rect.GetWidth() ) || 
                dstImage->GetHeight() != static_cast< unsigned int >( rect.GetHeight() ) )
            {
                dstImage.reset( new LinearImage
                    (
                    rect.GetWidth(),
                    rect.GetHeight(),
                    NULL,
                    ( ctx.mImage->GetNumChannels() == 4 ? (unsigned char*)1 : NULL )
                    ) );
            }

            ScaledImageFactory::ResultItem result;
            result.mGeneration = ctx.mGeneration;
            result.mRect = rect;
            GetScaledSubrect
                (
                *dstImage,
                *ctx.mImage,
                resamplers,
                rect.GetPosition()
                );
            result.mImage = new SrgbImage;
            dstImage->GetSrgb( result.mImage->mColor, result.mImage->mAlpha );
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

void ScaledImageFactory::SetImage( LinearImagePtr& newImage, double scale )
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
    mJobPool.Clear();

    // regenerate contrib lists
    mCurrentCtx.mContribLists = new Resampler::ContribLists
        (
        mCurrentCtx.mImage->GetWidth(), 
        mCurrentCtx.mImage->GetHeight(),
        mCurrentCtx.mImage->GetWidth() * newScale,
        mCurrentCtx.mImage->GetHeight() * newScale
        );
}

bool ScaledImageFactory::AddRect( const wxRect& rect )
{
    if( NULL == mCurrentCtx.mImage )
        throw std::runtime_error( "Image not set!" );

    return( wxSORTABLEMSGQUEUE_NO_ERROR == mJobPool.Post( JobItem( rect, mCurrentCtx ) ) );
}

bool ScaledImageFactory::GetImage( wxRect& rect, SrgbImagePtr& image )
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
