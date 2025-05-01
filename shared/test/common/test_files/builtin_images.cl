/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void fullCopy(__global const uint* src, __global uint* dst) {
    unsigned int gid = get_global_id(0);
    uint4 loaded = vload4(gid, src);
    vstore4(loaded, gid, dst);
}

#define ALIGNED4(ptr) __builtin_assume(((size_t)ptr&0b11) == 0)

__kernel void CopyBufferToBufferBytes(
    const __global uchar* pSrc,
    __global uchar* pDst,
    uint srcOffsetInBytes,
    uint dstOffsetInBytes,
    uint bytesToRead )
{
    ALIGNED4(pSrc);
    ALIGNED4(pDst);
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
    ALIGNED4(pSrc);
    ALIGNED4(pDst);
    unsigned int gid = get_global_id(0);
    pDst[ gid + dstOffsetInBytes ] = pSrc[ gid + srcOffsetInBytes ];
}

__kernel void CopyBufferToBufferMiddle(
    const __global uint* pSrc,
    __global uint* pDst,
    uint srcOffsetInBytes,
    uint dstOffsetInBytes)
{
    ALIGNED4(pSrc);
    ALIGNED4(pDst);
    unsigned int gid = get_global_id(0);
    pDst += dstOffsetInBytes >> 2;
    pSrc += srcOffsetInBytes >> 2;
    uint4 loaded = vload4(gid, pSrc);
    vstore4(loaded, gid, pDst);
}

__kernel void CopyBufferToBufferMiddleMisaligned(
    __global const uint* pSrc,
     __global uint* pDst,
     uint srcOffsetInBytes,
     uint dstOffsetInBytes,
     uint misalignmentInBits)
{
    ALIGNED4(pSrc);
    ALIGNED4(pDst);
    const size_t gid = get_global_id(0);
    pDst += dstOffsetInBytes >> 2;
    pSrc += srcOffsetInBytes >> 2;
    const uint4 src0 = vload4(gid, pSrc);
    const uint4 src1 = vload4(gid + 1, pSrc);

    uint4 result;
    result.x = (src0.x >> misalignmentInBits) | (src0.y << (32 - misalignmentInBits));
    result.y = (src0.y >> misalignmentInBits) | (src0.z << (32 - misalignmentInBits));
    result.z = (src0.z >> misalignmentInBits) | (src0.w << (32 - misalignmentInBits));
    result.w = (src0.w >> misalignmentInBits) | (src1.x << (32 - misalignmentInBits));
    vstore4(result, gid, pDst);
}

__kernel void CopyBufferToBufferRightLeftover(
    const __global uchar* pSrc,
    __global uchar* pDst,
    uint srcOffsetInBytes,
    uint dstOffsetInBytes)
{
    ALIGNED4(pSrc);
    ALIGNED4(pDst);
    unsigned int gid = get_global_id(0);
    pDst[ gid + dstOffsetInBytes ] = pSrc[ gid + srcOffsetInBytes ];
}

__kernel void copyBufferToBufferBytesSingle(__global uchar *dst, const __global uchar *src) {
    ALIGNED4(dst);
    ALIGNED4(src);
    unsigned int gid = get_global_id(0);
    dst[gid] = (uchar)(src[gid]);
}
__kernel void CopyBufferToBufferSideRegion(
    __global uchar* pDst,
    const __global uchar* pSrc,
    unsigned int len,
    uint dstSshOffset, // Offset needed in case ptr has been adjusted for SSH alignment
    uint srcSshOffset // Offset needed in case ptr has been adjusted for SSH alignment
    )
{
    ALIGNED4(pSrc);
    ALIGNED4(pDst);
    unsigned int gid = get_global_id(0);
    __global uchar* pDstWithOffset = (__global uchar*)((__global uchar*)pDst + dstSshOffset);
    __global uchar* pSrcWithOffset = (__global uchar*)((__global uchar*)pSrc + srcSshOffset);
    if (gid < len) {
        pDstWithOffset[ gid ] = pSrcWithOffset[ gid ];
    }
}

__kernel void CopyBufferToBufferMiddleRegion(
    __global uint* pDst,
    const __global uint* pSrc,
    unsigned int elems,
    uint dstSshOffset, // Offset needed in case ptr has been adjusted for SSH alignment
    uint srcSshOffset // Offset needed in case ptr has been adjusted for SSH alignment
    )
{
    ALIGNED4(pSrc);
    ALIGNED4(pDst);
    unsigned int gid = get_global_id(0);
    __global uint* pDstWithOffset = (__global uint*)((__global uchar*)pDst + dstSshOffset);
    __global uint* pSrcWithOffset = (__global uint*)((__global uchar*)pSrc + srcSshOffset);
    if (gid < elems) {
        uint4 loaded = vload4(gid, pSrcWithOffset);
        vstore4(loaded, gid, pDstWithOffset);
    }
}

#define ALIGNED4(ptr) __builtin_assume(((size_t)ptr&0b11) == 0)

// assumption is local work size = pattern size
__kernel void FillBufferBytes(
    __global uchar* pDst,
    uint dstOffsetInBytes,
    const __global uchar* pPattern )
{
    ALIGNED4(pDst);
    ALIGNED4(pPattern);
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
    ALIGNED4(pDst);
    ALIGNED4(pPattern);
    uint gid = get_global_id(0);
    pDst[ gid + dstOffsetInBytes ] = pPattern[ gid & (patternSizeInEls - 1) ];
}

__kernel void FillBufferMiddle(
    __global uchar* pDst,
    uint dstOffsetInBytes,
    const __global uint* pPattern,
    const uint patternSizeInEls )
{
    ALIGNED4(pDst);
    ALIGNED4(pPattern);
    uint gid = get_global_id(0);
    ((__global uint*)(pDst + dstOffsetInBytes))[gid] = pPattern[ gid & (patternSizeInEls - 1) ];
}

__kernel void FillBufferRightLeftover(
    __global uchar* pDst,
    uint dstOffsetInBytes,
    const __global uchar* pPattern,
    const uint patternSizeInEls )
{
    ALIGNED4(pDst);
    ALIGNED4(pPattern);
    uint gid = get_global_id(0);
    pDst[ gid + dstOffsetInBytes ] = pPattern[ gid & (patternSizeInEls - 1) ];
}

__kernel void FillBufferImmediate(
    __global uchar* ptr,
    ulong dstSshOffset, // Offset needed in case ptr has been adjusted for SSH alignment
    const uint value)
{
    ALIGNED4(ptr);
    uint gid = get_global_id(0);
    __global uint4* dstPtr = (__global uint4*)(ptr + dstSshOffset);
    dstPtr[gid] = value;
}

__kernel void FillBufferImmediateLeftOver(
    __global uchar* ptr,
    ulong dstSshOffset, // Offset needed in case ptr has been adjusted for SSH alignment
    const uint value)
{
    ALIGNED4(ptr);
    uint gid = get_global_id(0);
    (ptr + dstSshOffset)[gid] = value;
}

__kernel void FillBufferSSHOffset(
    __global uchar* ptr,
    uint dstSshOffset, // Offset needed in case ptr has been adjusted for SSH alignment
    const __global uchar* pPattern,
    uint patternSshOffset // Offset needed in case pPattern has been adjusted for SSH alignment
)
{
    ALIGNED4(ptr);
    ALIGNED4(pPattern);
    uint dstIndex = get_global_id(0);
    uint srcIndex = get_local_id(0);
    __global uchar* pDst = (__global uchar*)ptr + dstSshOffset;
    __global uchar* pSrc = (__global uchar*)pPattern + patternSshOffset;
    pDst[dstIndex] = pSrc[srcIndex];
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

void SetDstData(__global ulong* dst, uint currentOffset, ulong contextStart, ulong globalStart, ulong contextEnd, ulong globalEnd, uint useOnlyGlobalTimestamps) {
    dst[currentOffset] = globalStart;
    dst[currentOffset + 1] = globalEnd;
    if (useOnlyGlobalTimestamps != 0) {
        dst[currentOffset + 2] = globalStart;
        dst[currentOffset + 3] = globalEnd;
    } else {
        dst[currentOffset + 2] = contextStart;
        dst[currentOffset + 3] = contextEnd;
    }
}

ulong GetTimestampValue(ulong srcPtr, ulong timestampSizeInDw, uint index) {
    if(timestampSizeInDw == 1) {
        __global uint *src = (__global uint *) srcPtr;
        return src[index];
    } else if(timestampSizeInDw == 2) {
        __global ulong *src = (__global ulong *) srcPtr;
        return src[index];
    }

    return 0;
}

__kernel void QueryKernelTimestamps(__global ulong* srcEvents, __global ulong* dst, uint useOnlyGlobalTimestamps) {
    uint gid = get_global_id(0);
    uint currentOffset = gid * 4;
    dst[currentOffset] = 0;
    dst[currentOffset + 1] = 0;
    dst[currentOffset + 2] = 0;
    dst[currentOffset + 3] = 0;

    uint eventOffsetData = 3 * gid;

    ulong srcPtr = srcEvents[eventOffsetData];
    ulong packetUsed = srcEvents[eventOffsetData + 1];
    ulong timestampSizeInDw = srcEvents[eventOffsetData + 2];

    ulong contextStart = GetTimestampValue(srcPtr, timestampSizeInDw, 0);
    ulong globalStart = GetTimestampValue(srcPtr, timestampSizeInDw, 1);
    ulong contextEnd = GetTimestampValue(srcPtr, timestampSizeInDw, 2);
    ulong globalEnd = GetTimestampValue(srcPtr, timestampSizeInDw, 3);

    if(packetUsed > 1) {
        for(uint i = 1; i < packetUsed; i++) {
            uint timestampsOffsets = 4 * i;
            if(contextStart > GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets)) {
              contextStart = GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets);
            }
            if(globalStart > GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 1)) {
              globalStart = GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 1);
            }
            if(contextEnd < GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 2)) {
              contextEnd = GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 2);
            }
            if(globalEnd < GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 3)) {
              globalEnd = GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 3);
        }
      }
    }

    SetDstData(dst, currentOffset, contextStart, globalStart, contextEnd, globalEnd, useOnlyGlobalTimestamps);
}

__kernel void QueryKernelTimestampsWithOffsets(__global ulong* srcEvents, __global ulong* dst, __global ulong *offsets, uint useOnlyGlobalTimestamps) {
    uint gid = get_global_id(0);
    uint currentOffset = offsets[gid] / 8;
    dst[currentOffset] = 0;
    dst[currentOffset + 1] = 0;
    dst[currentOffset + 2] = 0;
    dst[currentOffset + 3] = 0;

    uint eventOffsetData = 3 * gid;

    ulong srcPtr = srcEvents[eventOffsetData];
    ulong packetUsed = srcEvents[eventOffsetData + 1];
    ulong timestampSizeInDw = srcEvents[eventOffsetData + 2];

    ulong contextStart = GetTimestampValue(srcPtr, timestampSizeInDw, 0);
    ulong globalStart = GetTimestampValue(srcPtr, timestampSizeInDw, 1);
    ulong contextEnd = GetTimestampValue(srcPtr, timestampSizeInDw, 2);
    ulong globalEnd = GetTimestampValue(srcPtr, timestampSizeInDw, 3);

    if(packetUsed > 1) {
        for(uint i = 1; i < packetUsed; i++) {
            uint timestampsOffsets = 4 * i;
            if(contextStart > GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets)) {
              contextStart = GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets);
            }
            if(globalStart > GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 1)) {
              globalStart = GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 1);
            }
            if(contextEnd < GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 2)) {
              contextEnd = GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 2);
            }
            if(globalEnd < GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 3)) {
              globalEnd = GetTimestampValue(srcPtr, timestampSizeInDw, timestampsOffsets + 3);
        }
      }
    }

    SetDstData(dst, currentOffset, contextStart, globalStart, contextEnd, globalEnd, useOnlyGlobalTimestamps);
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

__kernel void CopyImage3dToImage3d(
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

__kernel void CopyImage1dBufferToImage3d(
    __read_only image1d_buffer_t input,
    __write_only image3d_t output,
    int4 srcOffset,
    int4 dstOffset) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);

    const int4 srcCoord = (int4)(x, y, z, 0) + srcOffset;
    const int4 dstCoord = (int4)(x, y, z, 0) + dstOffset;
    const uint4 c = read_imageui(input, srcCoord.x);
    write_imageui(output, dstCoord, c);
}

__kernel void CopyImage3dToImage1dBuffer(
    __read_only image3d_t input,
    __write_only image1d_buffer_t output,
    int4 srcOffset,
    int4 dstOffset) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);

    const int4 srcCoord = (int4)(x, y, z, 0) + srcOffset;
    const int4 dstCoord = (int4)(x, y, z, 0) + dstOffset;
    const uint4 c = read_imageui(input, srcCoord);
    write_imageui(output, dstCoord.x, c);
}

__kernel void CopyImage1dBufferToImage1dBuffer(
    __read_only image1d_buffer_t input,
    __write_only image1d_buffer_t output,
    int4 srcOffset,
    int4 dstOffset) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);

    const int4 srcCoord = (int4)(x, y, z, 0) + srcOffset;
    const int4 dstCoord = (int4)(x, y, z, 0) + dstOffset;
    const uint4 c = read_imageui(input, srcCoord.x);
    write_imageui(output, dstCoord.x, c);
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

__kernel void FillImage1dBuffer(
    __write_only image1d_buffer_t output,
    uint4 color,
    int4 dstOffset) {
    const int x = get_global_id(0);

    const int dstCoord = x + dstOffset.x;
    write_imageui(output, dstCoord, color);
}