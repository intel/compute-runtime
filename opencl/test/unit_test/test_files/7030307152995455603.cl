/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void fullCopy(__global const uint* src, __global uint* dst) {
    unsigned int gid = get_global_id(0);
    uint4 loaded = vload4(gid, src);
    vstore4(loaded, gid, dst);
}

__kernel void CopyBufferToBufferBytes(
    const __global uchar* pSrc,
    __global uchar* pDst,
    uint srcOffsetInBytes,
    uint dstOffsetInBytes,
    uint bytesToRead )
{
    pSrc += ( srcOffsetInBytes + get_global_id(0) );
    pDst += ( dstOffsetInBytes + get_global_id(0) );
    pDst[ 0 ] = pSrc[ 0 ];
}

__kernel void CopyBufferToBufferLeftLeftover(
    const __global uchar* pSrc,
    __global uchar* pDst,
    uint srcOffsetInBytes,
    uint dstOffsetInBytes)
{
    unsigned int gid = get_global_id(0);
    pDst[ gid + dstOffsetInBytes ] = pSrc[ gid + srcOffsetInBytes ];
}

__kernel void CopyBufferToBufferMiddle(
    const __global uint* pSrc,
    __global uint* pDst,
    uint srcOffsetInBytes,
    uint dstOffsetInBytes)
{
    unsigned int gid = get_global_id(0);
    pDst += dstOffsetInBytes >> 2;
    pSrc += srcOffsetInBytes >> 2;
    uint4 loaded = vload4(gid, pSrc);
    vstore4(loaded, gid, pDst);
}

__kernel void CopyBufferToBufferRightLeftover(
    const __global uchar* pSrc,
    __global uchar* pDst,
    uint srcOffsetInBytes,
    uint dstOffsetInBytes)
{
    unsigned int gid = get_global_id(0);
    pDst[ gid + dstOffsetInBytes ] = pSrc[ gid + srcOffsetInBytes ];
}


// assumption is local work size = pattern size
__kernel void FillBufferBytes(
    __global uchar* pDst,
    uint dstOffsetInBytes,
    const __global uchar* pPattern )
{
    uint dstIndex = get_global_id(0) + dstOffsetInBytes;
    uint srcIndex = get_local_id(0);
    pDst[dstIndex] = pPattern[srcIndex];
}

__kernel void FillBufferLeftLeftover(
    __global uchar* pDst,
    uint dstOffsetInBytes,
    const __global uchar* pPattern,
    const uint patternSizeInEls )
{
    uint gid = get_global_id(0);
    pDst[ gid + dstOffsetInBytes ] = pPattern[ gid & (patternSizeInEls - 1) ];
}

__kernel void FillBufferMiddle(
    __global uchar* pDst,
    uint dstOffsetInBytes,
    const __global uint* pPattern,
    const uint patternSizeInEls )
{
    uint gid = get_global_id(0);
    ((__global uint*)(pDst + dstOffsetInBytes))[gid] = pPattern[ gid & (patternSizeInEls - 1) ];
}

__kernel void FillBufferRightLeftover(
    __global uchar* pDst,
    uint dstOffsetInBytes,
    const __global uchar* pPattern,
    const uint patternSizeInEls )
{
    uint gid = get_global_id(0);
    pDst[ gid + dstOffsetInBytes ] = pPattern[ gid & (patternSizeInEls - 1) ];
}

__kernel void FillImage1d(
    __write_only image1d_t output,
    uint4 color,
    int4 dstOffset) {
    const int x = get_global_id(0);

    const int dstCoord = x + dstOffset.x;
    write_imageui(output, dstCoord, color);
}

__kernel void FillImage2d(
    __write_only image2d_t output,
    uint4 color,
    int4 dstOffset) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);

    const int2 dstCoord = (int2)(x, y) + (int2)(dstOffset.x, dstOffset.y);
    write_imageui(output, dstCoord, color);
}

#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

__kernel void FillImage3d(
    __write_only image3d_t output,
    uint4 color,
    int4 dstOffset) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);

    const int4 dstCoord = (int4)(x, y, z, 0) + dstOffset;
    write_imageui(output, dstCoord, color);
}

__kernel void CopyImageToImage1d(
    __read_only image1d_t input,
    __write_only image1d_t output,
    int4 srcOffset,
    int4 dstOffset) {
    const int x = get_global_id(0);

    const int srcCoord = x + srcOffset.x;
    const int dstCoord = x + dstOffset.x;
    const uint4 c = read_imageui(input, srcCoord);
    write_imageui(output, dstCoord, c);
}

__kernel void CopyImageToImage2d(
    __read_only image2d_t input,
    __write_only image2d_t output,
    int4 srcOffset,
    int4 dstOffset) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);

    const int2 srcCoord = (int2)(x, y) + (int2)(srcOffset.x, srcOffset.y);
    const int2 dstCoord = (int2)(x, y) + (int2)(dstOffset.x, dstOffset.y);
    const uint4 c = read_imageui(input, srcCoord);
    write_imageui(output, dstCoord, c);
}

#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

__kernel void CopyImageToImage3d(
    __read_only image3d_t input,
    __write_only image3d_t output,
    int4 srcOffset,
    int4 dstOffset) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);

    const int4 srcCoord = (int4)(x, y, z, 0) + srcOffset;
    const int4 dstCoord = (int4)(x, y, z, 0) + dstOffset;
    const uint4 c = read_imageui(input, srcCoord);
    write_imageui(output, dstCoord, c);
}

//////////////////////////////////////////////////////////////////////////////
__kernel void CopyBufferRectBytes2d(
    __global const char* src,
    __global char* dst,
    uint4 SrcOrigin,
    uint4 DstOrigin,
    uint2 SrcPitch,
    uint2 DstPitch )

{
    int x = get_global_id(0);
    int y = get_global_id(1);

    uint LSrcOffset = x + SrcOrigin.x + ( ( y + SrcOrigin.y ) * SrcPitch.x );
    uint LDstOffset = x + DstOrigin.x + ( ( y + DstOrigin.y ) * DstPitch.x );

    *( dst + LDstOffset )  = *( src + LSrcOffset ); 

}
//////////////////////////////////////////////////////////////////////////////
__kernel void CopyBufferRectBytes3d(
    __global const char* src, 
    __global char* dst, 
    uint4 SrcOrigin, 
    uint4 DstOrigin, 
    uint2 SrcPitch, 
    uint2 DstPitch ) 
 
{ 
    int x = get_global_id(0); 
    int y = get_global_id(1); 
    int z = get_global_id(2); 
 
    uint LSrcOffset = x + SrcOrigin.x + ( ( y + SrcOrigin.y ) * SrcPitch.x ) + ( ( z + SrcOrigin.z ) * SrcPitch.y ); 
    uint LDstOffset = x + DstOrigin.x + ( ( y + DstOrigin.y ) * DstPitch.x ) + ( ( z + DstOrigin.z ) * DstPitch.y ); 
 
    *( dst + LDstOffset )  = *( src + LSrcOffset );  
 
}

#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

__kernel void CopyBufferToImage3dBytes(__global uchar *src,
                                       __write_only image3d_t output,
                                       int srcOffset,
                                       int4 dstOffset,
                                       uint2 Pitch) {
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint z = get_global_id(2);

    int4 dstCoord = (int4)(x, y, z, 0) + dstOffset;
    uint LOffset = srcOffset + (y * Pitch.x) + (z * Pitch.y);

    write_imageui(output, dstCoord, (uint4)(*(src + LOffset + x), 0, 0, 1));
}

__kernel void CopyBufferToImage3d2Bytes(__global uchar *src,
                                        __write_only image3d_t output,
                                        int srcOffset,
                                        int4 dstOffset,
                                        uint2 Pitch) {
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint z = get_global_id(2);

    int4 dstCoord = (int4)(x, y, z, 0) + dstOffset;
    uint LOffset = srcOffset + (y * Pitch.x) + (z * Pitch.y);

    uint4 c = (uint4)(0, 0, 0, 1);

    if(( ulong )(src + srcOffset) & 0x00000001){
        ushort upper = *((__global uchar*)(src + LOffset + x * 2 + 1));
        ushort lower = *((__global uchar*)(src + LOffset + x * 2));
        ushort combined = (upper << 8) | lower;
        c.x = (uint)combined;
    }
    else{
        c.x = (uint)(*(__global ushort*)(src + LOffset + x * 2));
    }
    write_imageui(output, dstCoord, c);
}

__kernel void CopyBufferToImage3d4Bytes(__global uchar *src,
                                        __write_only image3d_t output,
                                        int srcOffset,
                                        int4 dstOffset,
                                        uint2 Pitch) {
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint z = get_global_id(2);

    int4 dstCoord = (int4)(x, y, z, 0) + dstOffset;
    uint LOffset = srcOffset + (y * Pitch.x) + (z * Pitch.y);

    uint4 c = (uint4)(0, 0, 0, 1);

    if(( ulong )(src + srcOffset) & 0x00000003){
        uint upper2 = *((__global uchar*)(src + LOffset + x * 4 + 3));
        uint upper  = *((__global uchar*)(src + LOffset + x * 4 + 2));
        uint lower2 = *((__global uchar*)(src + LOffset + x * 4 + 1));
        uint lower  = *((__global uchar*)(src + LOffset + x * 4));
        uint combined = (upper2 << 24) | (upper << 16) | (lower2 << 8) | lower;
        c.x = combined;
    }
    else{
        c.x = (*(__global uint*)(src + LOffset + x * 4));
    }
    write_imageui(output, dstCoord, c);
}

__kernel void CopyBufferToImage3d8Bytes(__global uchar *src,
                                        __write_only image3d_t output,
                                        int srcOffset,
                                        int4 dstOffset,
                                        uint2 Pitch) {
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint z = get_global_id(2);

    int4 dstCoord = (int4)(x, y, z, 0) + dstOffset;
    uint LOffset = srcOffset + (y * Pitch.x) + (z * Pitch.y);

    uint2 c = (uint2)(0, 0);//*((__global uint2*)(src + LOffset + x * 8));

    if(( ulong )(src + srcOffset) & 0x00000007){
        uint upper2 = *((__global uchar*)(src + LOffset + x * 8 + 3));
        uint upper  = *((__global uchar*)(src + LOffset + x * 8 + 2));
        uint lower2 = *((__global uchar*)(src + LOffset + x * 8 + 1));
        uint lower  = *((__global uchar*)(src + LOffset + x * 8));
        uint combined = (upper2 << 24) | (upper << 16) | (lower2 << 8) | lower;
        c.x = combined;
        upper2 = *((__global uchar*)(src + LOffset + x * 8 + 7));
        upper  = *((__global uchar*)(src + LOffset + x * 8 + 6));
        lower2 = *((__global uchar*)(src + LOffset + x * 8 + 5));
        lower  = *((__global uchar*)(src + LOffset + x * 8 + 4));
        combined = ((uint)upper2 << 24) | ((uint)upper << 16) | ((uint)lower2 << 8) | lower;
        c.y = combined;
    }
    else{
        c = *((__global uint2*)(src + LOffset + x * 8));
    }

    write_imageui(output, dstCoord, (uint4)(c.x, c.y, 0, 1));
}

__kernel void CopyBufferToImage3d16Bytes(__global uchar *src,
                                         __write_only image3d_t output,
                                         int srcOffset,
                                         int4 dstOffset,
                                         uint2 Pitch) {
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint z = get_global_id(2);

    int4 dstCoord = (int4)(x, y, z, 0) + dstOffset;
    uint LOffset = srcOffset + (y * Pitch.x) + (z * Pitch.y);

    uint4 c = (uint4)(0, 0, 0, 0);

    if(( ulong )(src + srcOffset) & 0x0000000f){
        uint upper2 = *((__global uchar*)(src + LOffset + x * 16 + 3));
        uint upper  = *((__global uchar*)(src + LOffset + x * 16 + 2));
        uint lower2 = *((__global uchar*)(src + LOffset + x * 16 + 1));
        uint lower  = *((__global uchar*)(src + LOffset + x * 16));
        uint combined = (upper2 << 24) | (upper << 16) | (lower2 << 8) | lower;
        c.x = combined;
        upper2 = *((__global uchar*)(src + LOffset + x * 16 + 7));
        upper  = *((__global uchar*)(src + LOffset + x * 16 + 6));
        lower2 = *((__global uchar*)(src + LOffset + x * 16 + 5));
        lower  = *((__global uchar*)(src + LOffset + x * 16 + 4));
        combined = (upper2 << 24) | (upper << 16) | (lower2 << 8) | lower;
        c.y = combined;
        upper2 = *((__global uchar*)(src + LOffset + x * 16 + 11));
        upper  = *((__global uchar*)(src + LOffset + x * 16 + 10));
        lower2 = *((__global uchar*)(src + LOffset + x * 16 + 9));
        lower  = *((__global uchar*)(src + LOffset + x * 16 + 8));
        combined = (upper2 << 24) | (upper << 16) | (lower2 << 8) | lower;
        c.z = combined;
        upper2 = *((__global uchar*)(src + LOffset + x * 16 + 15));
        upper  = *((__global uchar*)(src + LOffset + x * 16 + 14));
        lower2 = *((__global uchar*)(src + LOffset + x * 16 + 13));
        lower  = *((__global uchar*)(src + LOffset + x * 16 + 12));
        combined = (upper2 << 24) | (upper << 16) | (lower2 << 8) | lower;
        c.w = combined;
    }
    else{
        c = *((__global uint4 *)(src + LOffset + x * 16));
    }

    write_imageui(output, dstCoord, c);
}

__kernel void CopyImage3dToBufferBytes(__read_only image3d_t input,
                                       __global uchar *dst,
                                       int4 srcOffset,
                                       int dstOffset,
                                       uint2 Pitch) {
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint z = get_global_id(2);

    const int4 srcCoord = (int4)(x, y, z, 0) + srcOffset;
    uint DstOffset = dstOffset + (y * Pitch.x) + (z * Pitch.y);

    uint4 c = read_imageui(input, srcCoord);
    *(dst + DstOffset + x) = convert_uchar_sat(c.x);
}

__kernel void CopyImage3dToBuffer2Bytes(__read_only image3d_t input,
                                        __global uchar *dst,
                                        int4 srcOffset,
                                        int dstOffset,
                                        uint2 Pitch) {
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint z = get_global_id(2);

    const int4 srcCoord = (int4)(x, y, z, 0) + srcOffset;
    uint DstOffset = dstOffset + (y * Pitch.x) + (z * Pitch.y);
    
    uint4 c = read_imageui(input, srcCoord);

    if(( ulong )(dst + dstOffset) & 0x00000001){
        *((__global uchar*)(dst + DstOffset + x * 2 + 1)) = convert_uchar_sat((c.x >> 8 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 2)) = convert_uchar_sat(c.x & 0xff);
    }
    else{
        *((__global ushort*)(dst + DstOffset + x * 2)) = convert_ushort_sat(c.x);
    }
}

__kernel void CopyImage3dToBuffer4Bytes(__read_only image3d_t input,
                                        __global uchar *dst,
                                        int4 srcOffset,
                                        int dstOffset,
                                        uint2 Pitch) {
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint z = get_global_id(2);

    const int4 srcCoord = (int4)(x, y, z, 0) + srcOffset;
    uint DstOffset = dstOffset + (y * Pitch.x) + (z * Pitch.y);

    uint4 c = read_imageui(input, srcCoord);

    if(( ulong )(dst + dstOffset) & 0x00000003){
        *((__global uchar*)(dst + DstOffset + x * 4 + 3)) = convert_uchar_sat((c.x >> 24 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 4 + 2)) = convert_uchar_sat((c.x >> 16 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 4 + 1)) = convert_uchar_sat((c.x >> 8 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 4)) = convert_uchar_sat(c.x & 0xff);
    }
    else{
        *((__global uint*)(dst + DstOffset + x * 4)) = c.x;
    }
}

__kernel void CopyImage3dToBuffer8Bytes(__read_only image3d_t input,
                                        __global uchar *dst,
                                        int4 srcOffset,
                                        int dstOffset,
                                        uint2 Pitch) {
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint z = get_global_id(2);

    const int4 srcCoord = (int4)(x, y, z, 0) + srcOffset;
    uint DstOffset = dstOffset + (y * Pitch.x) + (z * Pitch.y);

    uint4 c = read_imageui(input, srcCoord);

    if(( ulong )(dst + dstOffset) & 0x00000007){
        *((__global uchar*)(dst + DstOffset + x * 8 + 3)) = convert_uchar_sat((c.x >> 24 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 8 + 2)) = convert_uchar_sat((c.x >> 16 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 8 + 1)) = convert_uchar_sat((c.x >> 8 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 8)) = convert_uchar_sat(c.x & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 8 + 7)) = convert_uchar_sat((c.y >> 24 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 8 + 6)) = convert_uchar_sat((c.y >> 16 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 8 + 5)) = convert_uchar_sat((c.y >> 8 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 8 + 4)) = convert_uchar_sat(c.y & 0xff);
    }
    else{
        uint2 d = (uint2)(c.x,c.y);
        *((__global uint2*)(dst + DstOffset + x * 8)) = d;
    }
}

__kernel void CopyImage3dToBuffer16Bytes(__read_only image3d_t input,
                                         __global uchar *dst,
                                         int4 srcOffset,
                                         int dstOffset,
                                         uint2 Pitch) {
    const uint x = get_global_id(0);
    const uint y = get_global_id(1);
    const uint z = get_global_id(2);

    const int4 srcCoord = (int4)(x, y, z, 0) + srcOffset;
    uint DstOffset = dstOffset + (y * Pitch.x) + (z * Pitch.y);

    const uint4 c = read_imageui(input, srcCoord);

    if(( ulong )(dst + dstOffset) & 0x0000000f){
        *((__global uchar*)(dst + DstOffset + x * 16 + 3)) = convert_uchar_sat((c.x >> 24 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 2)) = convert_uchar_sat((c.x >> 16 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 1)) = convert_uchar_sat((c.x >> 8 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16)) = convert_uchar_sat(c.x & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 7)) = convert_uchar_sat((c.y >> 24 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 6)) = convert_uchar_sat((c.y >> 16 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 5)) = convert_uchar_sat((c.y >> 8 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 4)) = convert_uchar_sat(c.y & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 11)) = convert_uchar_sat((c.z >> 24 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 10)) = convert_uchar_sat((c.z >> 16 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 9)) = convert_uchar_sat((c.z >> 8 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 8)) = convert_uchar_sat(c.z & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 15)) = convert_uchar_sat((c.w >> 24 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 14)) = convert_uchar_sat((c.w >> 16 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 13)) = convert_uchar_sat((c.w >> 8 ) & 0xff);
        *((__global uchar*)(dst + DstOffset + x * 16 + 12)) = convert_uchar_sat(c.w & 0xff);
    }
    else{
        *(__global uint4*)(dst + DstOffset + x * 16) = c;
    }
}
