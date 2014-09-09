#ifndef SCALEDIMAGEFACTORY_H
#define SCALEDIMAGEFACTORY_H

#include <wx/wx.h>
#include <wx/msgqueue.h>

#include <utility>
#include <vector>

#include "imageresampler/resampler.h"


class LinearImage;
typedef wxSharedPtr< LinearImage > LinearImagePtr;


struct SrgbImage
{
    std::vector< unsigned char > mColor;
    std::vector< unsigned char > mAlpha;
};
typedef wxSharedPtr< SrgbImage > SrgbImagePtr;


class ScaledImageFactory : public wxThreadHelper
{
public:
    ScaledImageFactory( wxEvtHandler* eventSink, int id = wxID_ANY );
    ~ScaledImageFactory();
    void SetImage( LinearImagePtr& newImage, double scale );
    void SetScale( double newScale );
    bool AddRect( const wxRect& rect );
    bool GetImage( wxRect& rect, SrgbImagePtr& image );

private:
    // threadland
    virtual wxThread::ExitCode Entry();

    wxEvtHandler* mEventSink;
    int mEventId;

    struct Context
    {
        unsigned int mGeneration;
        LinearImagePtr mImage;
        wxSharedPtr< Resampler::ContribLists > mContribLists;
        wxSharedPtr< Resampler > mResamplers[ 4 ];
    };
    Context mCurrentCtx;

    typedef std::pair< wxRect, Context > JobItem;
    wxMessageQueue< JobItem > JobQueue;

    struct ResultItem
    {
        unsigned int mGeneration;
        wxRect mRect;
        SrgbImagePtr mImage;
    };
    wxMessageQueue< ResultItem > ResultQueue;
};

#endif