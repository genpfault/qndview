#ifndef SCALEDIMAGEFACTORY_H
#define SCALEDIMAGEFACTORY_H

#include <wx/wx.h>
#include <wx/msgqueue.h>
#include "wxSortableMsgQueue.h"

#include <utility>
#include <vector>
#include <list>


typedef wxSharedPtr< wxImage > wxImagePtr;

class WorkerThread;

class ScaledImageFactory
{
public:
    ScaledImageFactory( wxEvtHandler* eventSink, int id = wxID_ANY );
    ~ScaledImageFactory();
    void SetImage( wxImagePtr& newImage, double scale );
    void SetScale( double newScale );
    bool AddRect( const wxRect& rect );
    bool GetImage( wxRect& rect, wxImagePtr& image );
    void ClearQueue();

    // Sort the job queue with the given comparison functor
    template< class Compare >
    bool Sort( Compare comp )
    {
        return ( wxSORTABLEMSGQUEUE_NO_ERROR == mJobPool.Sort( JobItemCmp< Compare >( comp ) ) );
    }

private:
    friend WorkerThread;

    struct Context
    {
        unsigned int mGeneration;
        double mScale;
        wxImagePtr mImage;
    };
    Context mCurrentCtx;

    typedef std::pair< wxRect, Context > JobItem;
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
        wxRect mRect;
        wxImagePtr mImage;
    };
    typedef wxMessageQueue< ResultItem > ResultQueueType;
    ResultQueueType mResultQueue;

    std::list< wxThread* > mThreads;
};

#endif
