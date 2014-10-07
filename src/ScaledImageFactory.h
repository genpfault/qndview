#ifndef SCALEDIMAGEFACTORY_H
#define SCALEDIMAGEFACTORY_H

#include <wx/sharedptr.h>
#include <wx/image.h>
#include <wx/event.h>
#include <wx/msgqueue.h>
#include <tuple>

#include "wxSortableMsgQueue.h"
#include "wxMultiThreadHelper.h"


// (ab)use std::pair<>'s operator<() to compare wxRects
inline bool operator<( const wxRect& left, const wxRect& right )
{
    const std::pair< int, int >  leftPair(  left.GetTop(),  left.GetLeft() );
    const std::pair< int, int > rightPair( right.GetTop(), right.GetLeft() );
    return ( leftPair < rightPair );
}

// frame number, filter, rect
typedef std::tuple< size_t, int, wxRect > ExtRect;


class ScaledImageFactory : public wxMultiThreadHelper
{
public:
    typedef wxSharedPtr< wxImage > wxImagePtr;

    ScaledImageFactory( wxEvtHandler* eventSink, int id = wxID_ANY );
    ~ScaledImageFactory();
    void SetImage( wxImagePtr& newImage );
    void SetScale( double newScale );
    bool AddRect( const ExtRect& rect );
    bool GetImage( ExtRect& rect, wxImagePtr& image );
    void SetVisibleArea( const wxRect& visible );

    // Sort the job queue with the given comparison functor
    template< class Compare >
    bool Sort( Compare comp )
    {
        return ( wxSORTABLEMSGQUEUE_NO_ERROR == mJobPool.Sort( JobItemCmp< Compare >( comp ) ) );
    }

private:
    virtual wxThread::ExitCode Entry();

    struct Context
    {
        unsigned int mGeneration;
        double mScale;
        wxImagePtr mImage;
    };
    Context mCurrentCtx;

    typedef std::pair< ExtRect, Context > JobItem;
    typedef wxSortableMessageQueue< JobItem > JobPoolType;
    JobPoolType mJobPool;

    template< class Compare >
    struct JobItemCmp
    {
        Compare mComp;
        JobItemCmp( Compare comp ) : mComp( comp ) {}
        bool operator()( const JobItem& left, const JobItem& right )
        {
            return mComp( left.first, right.first );
        }
    };

    struct ResultItem
    {
        unsigned int mGeneration;
        ExtRect mRect;
        wxImagePtr mImage;
    };
    typedef wxMessageQueue< ResultItem > ResultQueueType;
    ResultQueueType mResultQueue;

    wxEvtHandler* mEventSink;
    int mEventId;

    wxRect mVisible;
    wxCriticalSection mVisibleCs;

    wxImage mStipple;
};

#endif
