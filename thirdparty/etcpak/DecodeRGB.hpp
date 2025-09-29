#ifndef __DECODERGB_HPP__
#define __DECODERGB_HPP__

#include <stddef.h>
#include <stdint.h>

void DecodeRBlock( const void* src, void* dst, size_t width );
void DecodeRGBlock( const void* src, void* dst, size_t width );
void DecodeRGBBlock( const void* src, void* dst, size_t width );
void DecodeRGBABlock( const void* src, void* dst, size_t width );
void DecodeR11SBlock( const void* src, void* dst, size_t width );
void DecodeRG11SBlock( const void* src, void* dst, size_t width );
void DecodeRGBA1Block( const void* src, void* dst, size_t width );

#endif
