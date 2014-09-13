#ifndef LINEARIMAGE_H
#define LINEARIMAGE_H

#include <cstddef>
#include <vector>

class LinearImage
{
public:
    LinearImage( const size_t width, const size_t height, const unsigned char* data, const unsigned char* alpha, const float gamma = 2.2f );

    void GetSrgb( std::vector< unsigned char >& color, std::vector< unsigned char >& alpha ) const;
    const float* GetRow( const size_t channel, const size_t row ) const;
    float* GetRow( const size_t channel, const size_t row );
    size_t GetWidth()     const { return mWidth;  }
    size_t GetHeight()    const { return mHeight; }
    size_t GetNumChannels() const { return mChannels.size(); }

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
