#include "LinearImage.h"


LinearImage::LinearImage( const size_t width, const size_t height, const bool hasAlpha, const unsigned char* data, const float gamma )
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


void LinearImage::GetSrgb( std::vector< unsigned char >& color, std::vector< unsigned char >& alpha ) const
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


const float* LinearImage::GetRow( const size_t channel, const size_t row ) const
{
    if( channel > mChannels.size() || row >= mHeight )
        return NULL;
    return &mChannels[ channel ][ mWidth * row ];
}


float* LinearImage::GetRow( const size_t channel, const size_t row )
{
    return const_cast< float* >( static_cast< const LinearImage& >( *this ).GetRow( channel, row ) );
}


void LinearImage::InitTables( const float gamma )
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