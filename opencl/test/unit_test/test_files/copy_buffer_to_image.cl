/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

__kernel void CopyBufferToImage3d(__global uchar *src,
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