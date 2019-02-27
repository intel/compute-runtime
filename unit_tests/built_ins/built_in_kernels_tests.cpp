/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/builtin_kernels_simulation/opencl_c.h"

#include "gtest/gtest.h"

//#include "unit_tests/test_files/4265157215134882557.cl"

namespace BuiltinKernelsSimulation {

__kernel void CopyImage3dToBuffer16Bytes(__read_only image3d_t input,
                                         __global uchar *dst,
                                         int4 srcOffset,
                                         int dstOffset,
                                         uint2 pitch) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);

    int4 srcCoord = {x, y, z, 0};
    srcCoord = srcCoord + srcOffset;
    uint DstOffset = dstOffset + (y * pitch.x) + (z * pitch.y);

    const uint4 c = read_imageui(input, srcCoord);

    if ((ulong)(dst + dstOffset) & 0x0000000f) {
        *((__global uchar *)(dst + DstOffset + x * 16 + 3)) = convert_uchar_sat((c.x >> 24) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 2)) = convert_uchar_sat((c.x >> 16) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 1)) = convert_uchar_sat((c.x >> 8) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16)) = convert_uchar_sat(c.x & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 7)) = convert_uchar_sat((c.y >> 24) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 6)) = convert_uchar_sat((c.y >> 16) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 5)) = convert_uchar_sat((c.y >> 8) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 4)) = convert_uchar_sat(c.y & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 11)) = convert_uchar_sat((c.z >> 24) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 10)) = convert_uchar_sat((c.z >> 16) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 9)) = convert_uchar_sat((c.z >> 8) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 8)) = convert_uchar_sat(c.z & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 15)) = convert_uchar_sat((c.w >> 24) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 14)) = convert_uchar_sat((c.w >> 16) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 13)) = convert_uchar_sat((c.w >> 8) & 0xff);
        *((__global uchar *)(dst + DstOffset + x * 16 + 12)) = convert_uchar_sat(c.w & 0xff);
    } else {
        *(__global uint4 *)(dst + DstOffset + x * 16) = c;
    }
}

TEST(BuiltInKernelTests, ReadImage) {

    uint width = 3;
    uint height = 3;
    uint depth = 3;
    uint bytesPerChannel = 4;
    uint channels = 4;

    uint bpp = bytesPerChannel * channels;

    globalID[0] = 0;
    globalID[1] = 0;
    globalID[2] = 0;
    localID[0] = 0;
    localID[1] = 0;
    localID[2] = 0;
    localSize[0] = width;
    localSize[1] = height;
    localSize[2] = depth;

    size_t size = width * height * depth * bytesPerChannel * channels;
    char *ptrSrc = new char[64 + size + 64];
    char *ptrDst = new char[64 + size + 64];
    char *ptrZero = new char[64];

    memset(ptrZero, 0, 64);
    memset(ptrDst, 0, 64 + size + 64);
    memset(ptrSrc, 0, 64 + size + 64);

    char *temp = ptrSrc + 64;

    for (uint i = 0; i < size; i++) {
        temp[i] = i;
    }

    image im;
    im.ptr = ptrSrc + 64;
    im.bytesPerChannel = bytesPerChannel;
    im.channels = channels;
    im.width = width;
    im.height = height;
    im.depth = depth;

    uint2 Pitch(0, 0);
    Pitch.x = width * bpp;
    Pitch.y = width * height * bpp;

    for (uint dimZ = 0; dimZ < depth; dimZ++) {
        globalID[1] = 0;
        for (uint dimY = 0; dimY < height; dimY++) {
            globalID[0] = 0;
            for (uint dimX = 0; dimX < width; dimX++) {

                CopyImage3dToBuffer16Bytes(&im,
                                           (uchar *)ptrDst + 64,
                                           {0, 0, 0, 0},
                                           0,
                                           Pitch);
                globalID[0]++;
            }
            globalID[1]++;
        }
        globalID[2]++;
    }

    EXPECT_EQ(0, memcmp(im.ptr, ptrDst + 64, size)) << "Data not copied properly!\n";

    EXPECT_EQ(0, memcmp(ptrDst, ptrZero, 64)) << "Data written before passed ptr!\n";
    EXPECT_EQ(0, memcmp(ptrDst + size + 64, ptrZero, 64)) << "Data written after passed ptr!\n";

    delete[] ptrSrc;
    delete[] ptrDst;
    delete[] ptrZero;
}
} // namespace BuiltinKernelsSimulation
