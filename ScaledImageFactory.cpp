#include "ScaledImageFactory.h"

#include <memory>

#include "LinearImage.h"

using namespace std;


ScaledImageFactory::ScaledImageFactory( wxEvtHandler* eventSink, int id )
    : mEventSink( eventSink )
    , mEventId( id )
{
    if( CreateThread( wxTHREAD_JOINABLE ) != wxTHREAD_NO_ERROR )
        throw std::runtime_error( "Could not create the worker thread!" );

    if( GetThread()->Run() != wxTHREAD_NO_ERROR )
        throw std::runtime_error( "Could not run the worker thread!" );

    mCurrentCtx.mGeneration = 0;
}

ScaledImageFactory::~ScaledImageFactory()
{
    // clear job queue and send down a "kill" job
    JobQueue.Clear();
    JobQueue.Post( JobItem( wxRect(), Context() ) );
    if( GetThread() && GetThread()->IsRunning() )
        GetThread()->Wait();
}

void ScaledImageFactory::SetImage( LinearImagePtr& newImage, double scale )
{
    JobQueue.Clear();
    const unsigned int nextGen = mCurrentCtx.mGeneration + 1;
    mCurrentCtx.mGeneration = nextGen;
    mCurrentCtx.mImage = newImage;
    SetScale( scale );
}

void ScaledImageFactory::SetScale( double newScale )
{
    if( NULL == mCurrentCtx.mImage )
        throw std::runtime_error( "Image not set!" );

    JobQueue.Clear();
    const unsigned int nextGen = mCurrentCtx.mGeneration + 1;
    mCurrentCtx.mGeneration = nextGen;

    // regenerate resamplers
    mCurrentCtx.mContribLists = new Resampler::ContribLists
        (
        mCurrentCtx.mImage->GetWidth(), 
        mCurrentCtx.mImage->GetHeight(),
        mCurrentCtx.mImage->GetWidth() * newScale,
        mCurrentCtx.mImage->GetHeight() * newScale
        );
    for( size_t i = 0; i < 4; ++i )
    {
        mCurrentCtx.mResamplers[ i ] = new Resampler( *mCurrentCtx.mContribLists );
    }
}

bool ScaledImageFactory::AddRect( const wxRect& rect )
{
    if( NULL == mCurrentCtx.mImage )
        throw std::runtime_error( "Image not set!" );

    wxMessageQueueError err;
    err = JobQueue.Post( JobItem( rect, mCurrentCtx ) );
    return( wxTHREAD_NO_ERROR == err );
}

bool ScaledImageFactory::GetImage( wxRect& rect, SrgbImagePtr& image )
{
    ResultItem item;
    wxMessageQueueError err;
    while( true )
    {
        err = ResultQueue.ReceiveTimeout( 0, item );
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


SrgbImagePtr GetScaledSubrect( const LinearImage& src, wxSharedPtr< Resampler > resamplers[ 4 ], const wxRect& rect )
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

    SrgbImagePtr ret( new SrgbImage );
    dst.GetSrgb( ret->mColor, ret->mAlpha );
    return ret;
}


// threadland
wxThread::ExitCode ScaledImageFactory::Entry()
{
    JobItem job;
    while( wxMSGQUEUE_NO_ERROR == JobQueue.Receive( job ) )
    {
        if( NULL == job.second.mImage || GetThread()->TestDestroy() )
            break;

        const wxRect& rect = job.first;
        Context& ctx = job.second;

        ResultItem result;
        result.mGeneration = ctx.mGeneration;
        result.mRect = rect;
        result.mImage = GetScaledSubrect
            (
            *ctx.mImage,
            ctx.mResamplers,
            rect
            );
        ResultQueue.Post( result );

        wxQueueEvent( mEventSink, new wxThreadEvent( wxEVT_THREAD, mEventId ) );
    }

    return static_cast< wxThread::ExitCode >( 0 );
}