/*
 * Copyright (C) 2022 Intel Corporation
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

__kernel void CopyBufferToBufferMiddleMisaligned(
    __global const uint* pSrc,
     __global uint* pDst,
     uint srcOffsetInBytes,
     uint dstOffsetInBytes,
     uint misalignmentInBits)
{
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
    ulong dstSshOffset, // Offset needed in case ptr has been adjusted for SSH alignment
    const uint value)
{
    uint gid = get_global_id(0);
    __global uint4* dstPtr = (__global uint4*)(ptr + dstSshOffset);
    dstPtr[gid] = value;
}

__kernel void FillBufferImmediateLeftOver(
    __global uchar* ptr,
    ulong dstSshOffset, // Offset needed in case ptr has been adjusted for SSH alignment
    const uint value)
{
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
