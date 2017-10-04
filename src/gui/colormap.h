#ifndef included_ColorMaps
#define included_ColorMaps

#include <stdint.h>


// Convert an unsigned rbg int to r,b,g char
inline void getRGB( uint32_t rgb, unsigned char& r, unsigned char& g, unsigned char& b )
{
    r = ( rgb >> 16 ) & 0xFF;
    g = ( rgb >> 8 ) & 0xFF;
    b = rgb & 0xFF;
}


// Convert r,b,g char to unsigned rgb int
inline uint32_t getRGB( unsigned char r, unsigned char g, unsigned char b )
{
    return ( 0xffu << 24 ) | ( r << 16 ) | ( g << 8 ) | b;
}


// Convert a double value (0:1) to uint rbg for QImage
// This uses the jet colormap (reverse order) with negitive values set as black
inline uint32_t jet( double value )
{
    uint r, g, b;
    if ( value < 0 ) {
        r = g = b = 0;
    } else if ( value < 0.125 ) {
        r = 255 * 4 * ( value + 0.125 );
        g = b = 0;
    } else if ( value < 0.375 ) {
        r = 255;
        g = 255 * 4 * ( value - 0.125 );
        b = 0;
    } else if ( value < 0.625 ) {
        r = 255 - 255 * 4 * ( value - 0.375 );
        g = 255;
        b = 255 * 4 * ( value - 0.375 );
    } else if ( value < 0.875 ) {
        r = 0;
        g = 255 - 255 * 4 * ( value - 0.625 );
        b = 255;
    } else if ( value < 1.0 ) {
        r = g = 0;
        b     = 255 - 255 * 4 * ( value - 0.875 );
    } else {
        r = g = 0;
        b     = 127;
    }
    return ( 0xffu << 24 ) | ( r << 16 ) | ( g << 8 ) | b;
}


#endif
