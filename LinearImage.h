#ifndef LINEARIMAGE_H
#define LINEARIMAGE_H

#include <vector>

class LinearImage
{
public:
    LinearImage( const size_t width, const size_t height, const bool hasAlpha, const unsigned char* data, const float gamma = 2.2f );

    void GetSrgb( std::vector< unsigned char >& color, std::vector< unsigned char >& alpha ) const;
    const float* GetRow( const size_t channel, const size_t row ) const;
    float* GetRow( const size_t channel, const size_t row );
    const size_t GetWidth()     const { return mWidth;  }
    const size_t GetHeight()    const { return mHeight; }
    const size_t GetNumChannels() const { return mChannels.size(); }

private:
    void InitTables( const float gamma );

    size_t mWidth, mHeight;
    typedef std::vector< float > Channel;
    typedef std::vector< Channel > Channels;
    Channels mChannels;
    std::vector< float > mSrgbToLinear;
    std::vector< unsigned char > mLinearToSrgb;
};

#endif