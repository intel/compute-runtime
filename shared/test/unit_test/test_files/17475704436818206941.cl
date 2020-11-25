/*
 * Copyright (C) 2020 Intel Corporation
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

__kernel void copyBufferToBufferBytesSingle(__global uchar *dst, const __global uchar *src) {
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
    unsigned int gid = get_global_id(0);
    __global uint* pDstWithOffset = (__global uint*)((__global uchar*)pDst + dstSshOffset);
    __global uint* pSrcWithOffset = (__global uint*)((__global uchar*)pSrc + srcSshOffset);
    if (gid < elems) {
        uint4 loaded = vload4(gid, pSrcWithOffset);
        vstore4(loaded, gid, pDstWithOffset);
    }
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

__kernel void FillBufferImmediate(
    __global uchar* ptr,
    uint dstSshOffset, // Offset needed in case ptr has been adjusted for SSH alignment
    const uint value)
{
    uint dstIndex = get_global_id(0);
    __global uchar* pDst = (__global uchar*)ptr + dstSshOffset;
    pDst[dstIndex] = value;
}

__kernel void FillBufferSSHOffset(
    __global uchar* ptr,
    uint dstSshOffset, // Offset needed in case ptr has been adjusted for SSH alignment
    const __global uchar* pPattern,
    uint patternSshOffset // Offset needed in case pPattern has been adjusted for SSH alignment
)
{
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

__kernel void QueryKernelTimestamps(__global ulong* srcEvents, __global ulong* dst, uint useOnlyGlobalTimestamps) {
    uint gid = get_global_id(0);
    const ulong tsMask = (1ull << 32) - 1;
    uint currentOffset = gid * 4;
    dst[currentOffset] = 0;
    dst[currentOffset + 1] = 0;
    dst[currentOffset + 2] = 0;
    dst[currentOffset + 3] = 0;

    ulong srcPtr = srcEvents[gid];
    __global uint *src = (__global uint *) srcPtr;
    dst[currentOffset] = src[1] & tsMask;
    dst[currentOffset + 1] = src[3] & tsMask;
    if (useOnlyGlobalTimestamps != 0) {
        dst[currentOffset + 2] = src[1] & tsMask;
        dst[currentOffset + 3] = src[3] & tsMask;
    } else {
        dst[currentOffset + 2] = src[0] & tsMask;
        dst[currentOffset + 3] = src[2] & tsMask;
    }
}

__kernel void QueryKernelTimestampsWithOffsets(__global ulong* srcEvents, __global ulong* dst, __global ulong *offsets, uint useOnlyGlobalTimestamps) {
    uint gid = get_global_id(0);
    const ulong tsMask = (1ull << 32) - 1;
    uint currentOffset = offsets[gid] / 8;
    dst[currentOffset] = 0;
    dst[currentOffset + 1] = 0;
    dst[currentOffset + 2] = 0;
    dst[currentOffset + 3] = 0;

    ulong srcPtr = srcEvents[gid];
    __global uint *src = (__global uint *) srcPtr;
    dst[currentOffset] = src[1] & tsMask;
    dst[currentOffset + 1] = src[3] & tsMask;
    if (useOnlyGlobalTimestamps != 0) {
        dst[currentOffset + 2] = src[1] & tsMask;
        dst[currentOffset + 3] = src[3] & tsMask;
    } else {
        dst[currentOffset + 2] = src[0] & tsMask;
        dst[currentOffset + 3] = src[2] & tsMask;
    }
}
