/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// VME KERNELS
__kernel __attribute__((reqd_work_group_size(16, 1, 1))) void
block_motion_estimate_intel(sampler_t accelerator, __read_only image2d_t srcImg,
                            __read_only image2d_t refImg,
                            __global short2 *prediction_motion_vector_buffer,
                            __global short2 *motion_vector_buffer,
                            __global ushort *residuals, int height, int width,
                            int stride) {
    __local uint dst[64];
    __local ushort *dist = (__local ushort *)&dst[8 * 5];

    int sid_0 = stride * get_group_id(0);
    int gid_0 = sid_0 / height;
    int gid_1 = sid_0 % height;
    for (int sid = sid_0; sid < sid_0 + stride && gid_0 < width && gid_1 < height;
         sid++, gid_0 = sid / height, gid_1 = sid % height) {
        int2 srcCoord = 0;
        int2 refCoord = 0;

        srcCoord.x = gid_0 * 16 + get_global_offset(0);
        srcCoord.y = gid_1 * 16 + get_global_offset(1);

        short2 predMV = 0;

#ifndef HW_NULL_CHECK
        if (prediction_motion_vector_buffer != NULL)
#endif
        {
            predMV = prediction_motion_vector_buffer[gid_0 + gid_1 * width];
            refCoord.x = predMV.x / 4;
            refCoord.y = predMV.y / 4;
            refCoord.y = refCoord.y & 0xFFFE;
        }

        {
            intel_work_group_vme_mb_query(dst, srcCoord, refCoord, srcImg, refImg,
                                          accelerator);
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        // Write Out Result

        // 4x4
        if (intel_get_accelerator_mb_block_type(accelerator) == 0x2) {
            int x = get_local_id(0) % 4;
            int y = get_local_id(0) / 4;
            int index = (gid_0 * 4 + x) + (gid_1 * 4 + y) * width * 4;

            short2 val = as_short2(dst[8 + (y * 4 + x) * 2]);
            motion_vector_buffer[index] = val;

#ifndef HW_NULL_CHECK
            if (residuals != NULL)
#endif
            {
                residuals[index] = dist[y * 4 + x];
            }
        }

        // 8x8
        if (intel_get_accelerator_mb_block_type(accelerator) == 0x1) {
            if (get_local_id(0) < 4) {
                int x = get_local_id(0) % 2;
                int y = get_local_id(0) / 2;
                int index = (gid_0 * 2 + x) + (gid_1 * 2 + y) * width * 2;
                short2 val = as_short2(dst[8 + (y * 2 + x) * 8]);
                motion_vector_buffer[index] = val;

#ifndef HW_NULL_CHECK
                if (residuals != NULL)
#endif
                {
                    residuals[index] = dist[(y * 2 + x) * 4];
                }
            }
        }

        // 16x16
        if (intel_get_accelerator_mb_block_type(accelerator) == 0x0) {
            if (get_local_id(0) == 0) {
                int index = gid_0 + gid_1 * width;

                short2 val = as_short2(dst[8]);
                motion_vector_buffer[index] = val;

#ifndef HW_NULL_CHECK
                if (residuals != NULL)
#endif
                {
                    residuals[index] = dist[0];
                }
            }
        }
    }
}

__kernel __attribute__((reqd_work_group_size(16, 1, 1))) void
block_advanced_motion_estimate_check_intel(
    sampler_t accelerator, __read_only image2d_t srcImg,
    __read_only image2d_t refImg, uint flags, uint skip_block_type,
    uint search_cost_penalty, uint search_cost_precision,
    __global short2 *count_motion_vector_buffer,
    __global short2 *predictors_buffer,
    __global short2 *skip_motion_vector_buffer,
    __global short2 *motion_vector_buffer,
    __global char *intra_search_predictor_modes, __global ushort *residuals,
    __global ushort *skip_residuals, __global ushort *intra_residuals,
    __read_only image2d_t intraSrcImg, int height, int width, int stride) {
    __local uint dstSearch[64];         // 8 GRFs
    __local uint dstSkipIntra[64 + 24]; // 11 GRFs (8 for inter, 3 for intra)

    __local ushort *distSearch =
        (__local ushort *)&dstSearch[8 * 5]; // distortion in the 6th GRF

    // Initialize the MV cost table:
    // MV Cost in U4U4 format:
    // No cost    : 0,  0,  0,  0,  0,  0,  0,  0
    // Low Cost   : 1,  4,  5,  9,  10, 12, 14, 15
    // Normal Cost: 5,  26, 29, 43, 45, 47, 57, 57
    // High Cost  : 29, 61, 72, 78, 88, 89, 91, 92

    uint2 MVCostTable;
    if (search_cost_penalty == 1) {
        MVCostTable.s0 = 0x09050401;
        MVCostTable.s1 = 0x0F0E0C0A;
    } else if (search_cost_penalty == 2) {
        MVCostTable.s0 = 0x2B1D1A05;
        MVCostTable.s1 = 0x39392F2D;
    } else if (search_cost_penalty == 3) {
        MVCostTable.s0 = 0x4E483D1D;
        MVCostTable.s1 = 0x5C5B5958;
    } else {
        MVCostTable.s0 = 0;
        MVCostTable.s1 = 0;
    }

    uint MVCostPrecision = ((uint)search_cost_precision) << 16;
    // Frame is divided into rows * columns of MBs.
    // One h/w thread per WG.
    // One WG processes 'row' MBs - one row per iteration and one MB per row.
    // Number of WGs (or h/w threads) is number of columns MBs
    // Each iteration processes the MB in a row - gid_0 is the MB id in a row and
    // gid_1 is the row offset.

    int sid_0 = stride * get_group_id(0);
    int gid_0 = sid_0 / height;
    int gid_1 = sid_0 % height;
    for (int sid = sid_0; sid < sid_0 + stride && gid_0 < width && gid_1 < height;
         sid++, gid_0 = sid / height, gid_1 = sid % height) {
        int2 srcCoord;

        srcCoord.x = gid_0 * 16 +
                     get_global_offset(0); // 16 pixels wide MBs (globally scalar)
        srcCoord.y = gid_1 * 16 +
                     get_global_offset(1); // 16 pixels tall MBs (globally scalar)

        uint curMB = gid_0 + gid_1 * width; // current MB id
        short2 count = count_motion_vector_buffer[curMB];

        int countPredMVs = count.x;
        if (countPredMVs != 0) {
            uint offset = curMB * 8;       // 8 predictors per MB
            offset += get_local_id(0) % 8; // 16 work-items access 8 MVs for MB
                                           // one predictor for MB per SIMD channel

            // Reduce predictors from Q-pixel to integer precision.

            int2 predMV = 0;
            if (get_local_id(0) < countPredMVs) {
                predMV =
                    convert_int2(predictors_buffer[offset]); // one MV per work-item
                predMV.x /= 4;
                predMV.y /= 4;
                predMV.y &= 0xFFFE;
            }

            // Do up to 8 IMEs, get the best MVs and their distortions, and optionally
            // a FBR of the best MVs.
            // Finally the results are written out to SLM.

            intel_work_group_vme_mb_multi_query_8(
                dstSearch,       // best search MV and its distortions into SLM
                countPredMVs,    // count of predictor MVs (globally scalar - value range
                                 // 1 to 8)
                MVCostPrecision, // MV cost precision
                MVCostTable,     // MV cost table
                srcCoord,        // MB 2-D offset (globally scalar)
                predMV,          // predictor MVs (up to 8 distinct MVs for SIMD16 thread)
                srcImg,          // source
                refImg,          // reference
                accelerator);    // vme object
        }

        int doIntra = (flags & 0x2) != 0;
        int intraEdges = 0;
        if (doIntra) {
            // Enable all edges by default.
            intraEdges = 0x3C;
            // If this is a left-edge MB, then disable left edges.
            if ((gid_0 == 0) & (get_global_offset(0) == 0)) {
                intraEdges &= 0x14;
            }
            // If this is a right edge MB then disable right edges.
            if (gid_0 == width - 1) {
                intraEdges &= 0x38;
            }
            // If this is a top-edge MB, then disable top edges.
            if ((gid_1 == 0) & (get_global_offset(1) == 0)) {
                intraEdges &= 0x20;
            }
            // Set bit6=bit5.
            intraEdges |= ((intraEdges & 0x20) << 1);
            intraEdges <<= 8;
        }
        int countSkipMVs = count.y;
        if (countSkipMVs != 0 || doIntra == true) {
            uint offset = curMB * 8; // 8 sets of skip check MVs per MB
            offset +=
                (get_local_id(0) % 8); // 16 work-items access 8 sets of MVs for MB
                                       // one set of skip MV per SIMD channel

            // Do up to 8 skip checks and get the distortions for each of them.
            // Finally the results are written out to SLM.

            if ((skip_block_type == 0x0) | ((doIntra) & (countSkipMVs == 0))) {
                int skipMVs = 0;
                if (get_local_id(0) < countSkipMVs) {
                    __global int *skip1_motion_vector_buffer =
                        (__global int *)skip_motion_vector_buffer;
                    skipMVs = skip1_motion_vector_buffer[offset]; // one packed MV for one
                                                                  // work-item
                }
                intel_work_group_vme_mb_multi_check_16x16(
                    dstSkipIntra, // distortions into SLM
                    countSkipMVs, // count of skip check MVs (value range 0 to 8)
                    doIntra,      // compute intra modes
                    intraEdges,   // intra edges to use
                    srcCoord,     // MB 2-D offset (globally scalar)
                    skipMVs,      // skip check MVs (up to 8 sets of skip check MVs for
                                  // SIMD16 thread)
                    srcImg,       // source
                    refImg,       // reference
                    intraSrcImg,  // intra source
                    accelerator);
            }

            if ((skip_block_type == 0x1) & (countSkipMVs > 0)) {
                int4 skipMVs = 0;
                if (get_local_id(0) < countSkipMVs) {
                    __global int4 *skip4_motion_vector_buffer =
                        (__global int4 *)(skip_motion_vector_buffer);
                    skipMVs = skip4_motion_vector_buffer[offset]; // four component MVs
                                                                  // per work-item
                }
                intel_work_group_vme_mb_multi_check_8x8(
                    dstSkipIntra, // distortions into SLM
                    countSkipMVs, // count of skip check MVs per MB (value range 0 to 8)
                    doIntra,      // compute intra modes
                    intraEdges,   // intra edges to use
                    srcCoord,     // MB 2-D offset (globally scalar)
                    skipMVs,      // skip check MVs (up to 8 ets of skip check MVs for SIMD16
                                  // thread)
                    srcImg,       // source
                    refImg,       // reference
                    intraSrcImg,  // intra source
                    accelerator);
            }
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        // Write Out motion estimation result:
        // Result format
        //     Hierarchical row-major layout
        //     i.e. row-major of blocks MVs in MBs, and row-major of 8 sets of
        //     MVs/distortion in blocks

        if (countPredMVs != 0) {
            // 4x4
            if (intel_get_accelerator_mb_block_type(accelerator) == 0x2) {
                int index = (gid_0 * 16 + get_local_id(0)) + (gid_1 * 16 * width);

                // 1. 16 work-items enabled.
                // 2. Work-items gather fwd MVs in strided dword locations 0, 2, .., 30
                // (interleaved
                //    fwd/bdw MVs) with constant offset 8 (control data size) from SLM
                //    into contiguous
                //    short2 locations 0, 1, .., 15 of global buffer
                //    search_motion_vector_buffer with
                //    offset index.
                // 3. Work-items gather contiguous ushort locations 0, 1, .., 15 from
                // distSearch into
                //    contiguous ushort locations 0, 1, .., 15 of search_residuals with
                //    offset index.

                short2 val = as_short2(dstSearch[8 + get_local_id(0) * 2]);
                motion_vector_buffer[index] = val;

#ifndef HW_NULL_CHECK
                if (residuals != NULL)
#endif
                {
                    residuals[index] = distSearch[get_local_id(0)];
                }
            }

            // 8x8
            else if (intel_get_accelerator_mb_block_type(accelerator) == 0x1) {
                // Only 1st 4 work-item are needed.
                if (get_local_id(0) < 4) {
                    int index = (gid_0 * 4 + get_local_id(0)) + (gid_1 * 4 * width);

                    // 1. 4 work-items enabled.
                    // 2. Work-items gather fw MVs in strided dword locations 0, 8, 16, 24
                    // (interleaved
                    //    fwd/bdw MVs) with constant offset 8 from SLM into contiguous
                    //    short2 locations
                    //    0, 1, .., 15 of global buffer search_motion_vector_buffer with
                    //    offset index.
                    // 3. Work-items gather strided ushort locations 0, 4, 8, 12 from
                    // distSearch into
                    //    contiguous ushort locations 0, 1, .., 15 of search_residuals
                    //    with offset index.

                    short2 val = as_short2(dstSearch[8 + get_local_id(0) * 4 * 2]);
                    motion_vector_buffer[index] = val;

#ifndef HW_NULL_CHECK
                    if (residuals != NULL)
#endif
                    {
                        residuals[index] = distSearch[get_local_id(0) * 4];
                    }
                }
            }

            // 16x16
            else if (intel_get_accelerator_mb_block_type(accelerator) == 0x0) {
                // One 1st work is needed.
                if (get_local_id(0) == 0) {
                    int index = gid_0 + gid_1 * width;

                    // 1. 1 work-item enabled.
                    // 2. Work-item gathers fwd MV in dword location 0 with constant
                    // offset 8 from
                    //    SLM into short2 locations 0 of global buffer
                    //    search_motion_vector_buffer.
                    // 3. Work-item gathers ushort location 0 from distSearch into ushort
                    //    location 0 of search_residuals with offset index.

                    short2 val = as_short2(dstSearch[8]);
                    motion_vector_buffer[index] = val;

#ifndef HW_NULL_CHECK
                    if (residuals != NULL)
#endif
                    {
                        residuals[index] = distSearch[0];
                    }
                }
            }
        }

        // Write out motion skip check result:
        // Result format
        //     Hierarchical row-major layout
        //     i.e. row-major of blocks in MBs, and row-major of 8 sets of
        //     distortions in blocks

        if (countSkipMVs != 0) {
            if (skip_block_type == 0x0) {
                // Copy out 8 (1 component) sets of distortion values.

                int index = (gid_0 * 8) + (get_local_id(0)) + (gid_1 * 8 * width);

                if (get_local_id(0) < countSkipMVs) {
                    __local ushort *distSkip = (__local ushort *)&dstSkipIntra[0];

                    // 1. Up to 8 work-items are enabled.
                    // 2. The work-item gathers distSkip locations 0, 16*1, .., 16*7 and
                    //    copies them to contiguous skip_residual locations 0, 1, 2, ..,
                    //    7.
                    skip_residuals[index] = distSkip[get_local_id(0) * 16];
                }
            } else {
                // Copy out 8 (4 component) sets of distortion values.

                int index =
                    (gid_0 * 8 * 4) + (get_local_id(0)) + (gid_1 * 8 * 4 * width);

                __local ushort *distSkip = (__local ushort *)&dstSkipIntra[0];

                if (get_local_id(0) < countSkipMVs * 4) {
                    // 1. Up to 16 work-items are enabled.
                    // 2. The work-item gathers distSkip locations 0, 4*1, .., 4*31 and
                    //    copies them to contiguous skip_residual locations 0, 1, 2, ..,
                    //    31.

                    skip_residuals[index] = distSkip[get_local_id(0) * 4];
                    skip_residuals[index + 16] = distSkip[(get_local_id(0) + 16) * 4];
                }
            }
        }

        // Write out intra search result:

        if (doIntra) {

            int index_low =
                (gid_0 * 22) + (get_local_id(0) * 2) + (gid_1 * 22 * width);
            int index_high =
                (gid_0 * 22) + (get_local_id(0) * 2) + 1 + (gid_1 * 22 * width);

            // Write out the 4x4 intra modes
            if (get_local_id(0) < 8) {
                __local char *dstIntra_4x4 =
                    (__local char *)(&dstSkipIntra[64 + 16 + 4]);
                char value = dstIntra_4x4[get_local_id(0)];
                char value_low = (value)&0xf;
                char value_high = (value >> 4) & 0xf;
                intra_search_predictor_modes[index_low + 5] = value_low;
                intra_search_predictor_modes[index_high + 5] = value_high;
            }

            // Write out the 8x8 intra modes
            if (get_local_id(0) < 4) {
                __local char *dstIntra_8x8 =
                    (__local char *)(&dstSkipIntra[64 + 8 + 4]);
                char value = dstIntra_8x8[get_local_id(0) * 2];
                char value_low = (value)&0xf;
                int index = (gid_0 * 22) + (get_local_id(0)) + (gid_1 * 22 * width);
                intra_search_predictor_modes[index + 1] = value_low;
            }

            // Write out the 16x16 intra modes
            if (get_local_id(0) < 1) {
                __local char *dstIntra_16x16 =
                    (__local char *)(&dstSkipIntra[64 + 0 + 4]);
                char value = dstIntra_16x16[get_local_id(0)];
                char value_low = (value)&0xf;
                intra_search_predictor_modes[index_low] = value_low;
            }

// Get the intra residuals.
#ifndef HW_NULL_CHECK
            if (intra_residuals != NULL)
#endif
            {
                int index = (gid_0 * 4) + (gid_1 * 4 * width);

                if (get_local_id(0) < 1) {
                    __local ushort *distIntra_4x4 =
                        (__local ushort *)(&dstSkipIntra[64 + 16 + 3]);
                    __local ushort *distIntra_8x8 =
                        (__local ushort *)(&dstSkipIntra[64 + 8 + 3]);
                    __local ushort *distIntra_16x16 =
                        (__local ushort *)(&dstSkipIntra[64 + 0 + 3]);
                    intra_residuals[index + 2] = distIntra_4x4[0];
                    intra_residuals[index + 1] = distIntra_8x8[0];
                    intra_residuals[index + 0] = distIntra_16x16[0];
                }
            }
        }
    }
}
/*************************************************************************************************
Built-in kernel:
   block_advanced_motion_estimate_bidirectional_check_intel

Description:

   1. Do motion estimation with 0 to 4 predictor MVs using 0 to 4 (integer
motion estimation)
   IMEs per macro-block, calculating the best search MVs per specified (16x16,
8x8, 4x4) luma
   block with lowest distortion from amongst the 0 to 4 IME results, and
optionally do
   (fractional bi-directional refinement) FBR on the best IME search results to
refine the best
   search results. The best search (FBR if done, or IME) MVs and their
distortions are returned.

   2. Do undirectional or bidirectional skip (zero search) checks with 0 to 4
sets of skip check
   MVs for (16x16, 8x8) luma blocks using 0 to 4 (skip and intra check) SICs and
return the
   distortions associated with the input sets of skip check MVs per specified
luma block. 4x4
   blocks are not supported by h/w for skip checks.

   3. Do intra-prediction for (16x16, 8x8, 4x4) luma blocks and (8x8) chroma
blocks using 3 SICs
   and returning the predictor modes and their associated distortions.
Intra-prediction is done
   for all block sizes. Support for 8x8 chroma blocks cannot be enabled until NV
image formats
   are supported in OCL.

**************************************************************************************************/
__kernel __attribute__((reqd_work_group_size(16, 1, 1))) void
block_advanced_motion_estimate_bidirectional_check_intel(
    sampler_t accelerator, __read_only image2d_t srcImg,
    __read_only image2d_t refImg, __read_only image2d_t src_check_image,
    __read_only image2d_t ref0_check_image,
    __read_only image2d_t ref1_check_image, uint flags,
    uint search_cost_penalty, uint search_cost_precision, short2 count_global,
    uchar bidir_weight, __global short2 *count_motion_vector_buffer,
    __global short2 *prediction_motion_vector_buffer,
    __global char *skip_input_mode_buffer,
    __global short2 *skip_motion_vector_buffer,
    __global short2 *search_motion_vector_buffer,
    __global char *intra_search_predictor_modes,
    __global ushort *search_residuals, __global ushort *skip_residuals,
    __global ushort *intra_residuals, __read_only image2d_t intraSrcImg,
    int height, int width, int stride) {
    __local uint dstSearch[64];         // 8 GRFs
    __local uint dstSkipIntra[32 + 24]; // 7 GRFs (4 for inter, 3 for intra)

    // distortion in the 6th GRF
    __local ushort *distSearch = (__local ushort *)&dstSearch[8 * 5];

    // Initialize the MV cost table:
    // MV Cost in U4U4 format:
    // No cost    : 0,  0,  0,  0,  0,  0,  0,  0
    // Low Cost   : 1,  4,  5,  9,  10, 12, 14, 15
    // Normal Cost: 5,  26, 29, 43, 45, 47, 57, 57
    // High Cost  : 29, 61, 72, 78, 88, 89, 91, 92

    uint2 MVCostTable;
    if (search_cost_penalty == 1) {
        MVCostTable.s0 = 0x09050401;
        MVCostTable.s1 = 0x0F0E0C0A;
    } else if (search_cost_penalty == 2) {
        MVCostTable.s0 = 0x2B1D1A05;
        MVCostTable.s1 = 0x39392F2D;
    } else if (search_cost_penalty == 3) {
        MVCostTable.s0 = 0x4E483D1D;
        MVCostTable.s1 = 0x5C5B5958;
    } else {
        MVCostTable.s0 = 0;
        MVCostTable.s1 = 0;
    }

    uint MVCostPrecision = ((uint)search_cost_precision) << 16;

    // Frame is divided into rows * columns of MBs.
    // One h/w thread per WG.
    // One WG processes "row" MBs - one row per iteration and one MB per row.
    // Number of WGs (or h/w threads) is number of columns MBs.Each iteration
    // processes the MB in a row - gid_0 is the MB id in a row and gid_1 is the
    // row offset.

    int sid_0 = stride * get_group_id(0);
    int gid_0 = sid_0 / height;
    int gid_1 = sid_0 % height;
    for (int sid = sid_0; sid < sid_0 + stride && gid_0 < width && gid_1 < height;
         sid++, gid_0 = sid / height, gid_1 = sid % height) {
        int2 srcCoord;

        srcCoord.x = gid_0 * 16 +
                     get_global_offset(0); // 16 pixels wide MBs (globally scalar)
        srcCoord.y = gid_1 * 16 +
                     get_global_offset(1);  // 16 pixels tall MBs (globally scalar)
        uint curMB = gid_0 + gid_1 * width; // current MB id
        short2 count;

        // If either the search or skip vector counts are per-MB, then we need to
        // read in
        // the count motion vector buffer.

        if ((count_global.s0 == -1) | (count_global.s1 == -1)) {
            count = count_motion_vector_buffer[curMB];
        }

        // If either the search or skip vector counts are per-frame, we need to use
        // those.

        if (count_global.s0 >= 0) {
            count.s0 = count_global.s0;
        }

        if (count_global.s1 >= 0) {
            count.s1 = count_global.s1;
        }

        int countPredMVs = count.x;
        if (countPredMVs != 0) {
            uint offset = curMB * 4;       // 4 predictors per MB
            offset += get_local_id(0) % 4; // 16 work-items access 4 MVs for MB
            // one predictor for MB per SIMD channel

            // Reduce predictors from Q-pixel to integer precision.
            int2 predMV = 0;

            if (get_local_id(0) < countPredMVs) {
                // one MV per work-item
                predMV = convert_int2(prediction_motion_vector_buffer[offset]);
                // Predictors are input in QP resolution. Convert that to integer
                // resolution.
                predMV.x /= 4;
                predMV.y /= 4;
                predMV.y &= 0xFFFFFFFE;
            }

            // Do up to 4 IMEs, get the best MVs and their distortions, and optionally
            // a FBR of
            // the best MVs. Finally the results are written out to SLM.

            intel_work_group_vme_mb_multi_query_4(
                dstSearch,       // best search MV and its distortions into SLM
                countPredMVs,    // count of predictor MVs (globally scalar - value range
                                 // 1 to 4)
                MVCostPrecision, // MV cost precision
                MVCostTable,     // MV cost table
                srcCoord,        // MB 2-D offset (globally scalar)
                predMV,          // predictor MVs (up to 4 distinct MVs for SIMD16 thread)
                srcImg,          // source
                refImg,          // reference
                accelerator);    // vme object
        }

        int doIntra = ((flags & 0x2) != 0);
        int intraEdges = 0;
        if (doIntra) {
            // Enable all edges by default.
            intraEdges = 0x3C;
            // If this is a left-edge MB, then disable left edges.
            if ((gid_0 == 0) & (get_global_offset(0) == 0)) {
                intraEdges &= 0x14;
            }

            // If this is a right edge MB then disable right edges.
            if (gid_0 == width - 1) {
                intraEdges &= 0x38;
            }

            // If this is a top-edge MB, then disable top edges.
            if ((gid_1 == 0) & (get_global_offset(1) == 0)) {
                intraEdges &= 0x20;
            }

            // Set bit6=bit5.
            intraEdges |= ((intraEdges & 0x20) << 1);

            intraEdges <<= 8;
        }

        int skip_block_type_8x8 = flags & 0x4;

        int countSkipMVs = count.y;
        if (countSkipMVs != 0 || doIntra == true) {
            // one set of skip MV per SIMD channel

            // Do up to 4 skip checks and get the distortions for each of them.
            // Finally the results are written out to SLM.

            if ((skip_block_type_8x8 == 0) | ((doIntra) & (countSkipMVs == 0))) {
                // 16x16:

                uint offset = curMB * 4 * 2; // 4 sets of skip check MVs per MB
                int skipMV = 0;
                if (get_local_id(0) < countSkipMVs * 2) // need 2 values per MV
                {
                    offset +=
                        (get_local_id(0)); // 16 work-items access 4 sets of MVs for MB
                    __global int *skip1_motion_vector_buffer =
                        (__global int *)skip_motion_vector_buffer;
                    skipMV = skip1_motion_vector_buffer[offset]; // one MV per work-item
                }

                uchar skipMode = 0;
                if (get_local_id(0) < countSkipMVs) {
                    skipMode = skip_input_mode_buffer[curMB];

                    if (skipMode == 0) {
                        skipMode = 1;
                    }
                    if (skipMode > 3) {
                        skipMode = 3;
                    }
                }

                intel_work_group_vme_mb_multi_bidir_check_16x16(
                    dstSkipIntra,     // distortions into SLM
                    countSkipMVs,     // count of skip check MVs (globally scalar - value
                                      // range 1 to 4)
                    doIntra,          // compute intra modes
                    intraEdges,       // intra edges to use
                    srcCoord,         // MB 2-D offset (globally scalar)
                    bidir_weight,     // bidirectional weight
                    skipMode,         // skip modes
                    skipMV,           // skip check MVs (up to 4 distinct sets of skip check MVs
                                      // for SIMD16 thread)
                    src_check_image,  // source
                    ref0_check_image, // reference fwd
                    ref1_check_image, // reference bwd
                    intraSrcImg,      // intra source
                    accelerator);     // vme object
            } else {
                // 8x8:

                uint offset =
                    curMB * 4 *
                    8; // 4 sets of skip check MVs, 16 shorts (8 ints) each per MB
                int2 skipMVs = 0;
                if (get_local_id(0) < countSkipMVs * 8) // need 8 values per MV
                {
                    offset +=
                        (get_local_id(0)); // 16 work-items access 4 sets of MVs for MB
                    __global int *skip1_motion_vector_buffer =
                        (__global int *)(skip_motion_vector_buffer);
                    skipMVs.x = skip1_motion_vector_buffer[offset]; // four component MVs
                                                                    // per work-item
                    skipMVs.y = skip1_motion_vector_buffer[offset + 16];
                }

                uchar skipModes = 0;
                if (get_local_id(0) < countSkipMVs) {
                    skipModes = skip_input_mode_buffer[curMB];
                }

                intel_work_group_vme_mb_multi_bidir_check_8x8(
                    dstSkipIntra,     // distortions into SLM
                    countSkipMVs,     // count of skip check MVs per MB (globally scalar -
                                      // value range 1 to 4)
                    doIntra,          // compute intra modes
                    intraEdges,       // intra edges to use
                    srcCoord,         // MB 2-D offset (globally scalar)
                    bidir_weight,     // bidirectional weight
                    skipModes,        // skip modes
                    skipMVs,          // skip check MVs (up to 4 distinct sets of skip check MVs
                                      // for SIMD16 thread)
                    src_check_image,  // source
                    ref0_check_image, // reference fwd
                    ref1_check_image, // reference bwd
                    intraSrcImg,      // intra source
                    accelerator);     // vme object
            }
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        // Write Out motion estimation result:
        // Result format
        //     Hierarchical row-major layout
        //     i.e. row-major of blocks MVs in MBs, and row-major of 4 sets of
        //     MVs/distortion in blocks
        if (countPredMVs != 0) {
            // 4x4
            if (intel_get_accelerator_mb_block_type(accelerator) == 0x2) {
                int index = (gid_0 * 16 + get_local_id(0)) + (gid_1 * 16 * width);

                // 1. 16 work-items enabled.
                // 2. Work-items gather fwd MVs in strided dword locations 0, 2, .., 30
                // (interleaved
                //    fwd/bdw MVs) with constant offset 8 (control data size) from SLM
                //    into contiguous
                //    short2 locations 0, 1, .., 15 of global buffer
                //    search_motion_vector_buffer with
                //    offset index.
                // 3. Work-items gather contiguous ushort locations 0, 1, .., 15 from
                // distSearch into
                //    contiguous ushort locations 0, 1, .., 15 of search_residuals with
                //    offset index.

                short2 val = as_short2(dstSearch[8 + get_local_id(0) * 2]);
                search_motion_vector_buffer[index] = val;

#ifndef HW_NULL_CHECK
                if (search_residuals != NULL)
#endif
                {
                    search_residuals[index] = distSearch[get_local_id(0)];
                }
            }

            // 8x8
            else if (intel_get_accelerator_mb_block_type(accelerator) == 0x1) {
                // Only 1st 4 work-item are needed.
                if (get_local_id(0) < 4) {
                    int index = (gid_0 * 4 + get_local_id(0)) + (gid_1 * 4 * width);

                    // 1. 4 work-items enabled.
                    // 2. Work-items gather fw MVs in strided dword locations 0, 8, 16, 24
                    // (interleaved
                    //    fwd/bdw MVs) with constant offset 8 from SLM into contiguous
                    //    short2 locations
                    //    0, 1, .., 15 of global buffer search_motion_vector_buffer with
                    //    offset index.
                    // 3. Work-items gather strided ushort locations 0, 4, 8, 12 from
                    // distSearch into
                    //    contiguous ushort locations 0, 1, .., 15 of search_residuals
                    //    with offset index.

                    short2 val = as_short2(dstSearch[8 + get_local_id(0) * 4 * 2]);
                    search_motion_vector_buffer[index] = val;

#ifndef HW_NULL_CHECK
                    if (search_residuals != NULL)
#endif
                    {
                        search_residuals[index] = distSearch[get_local_id(0) * 4];
                    }
                }
            }

            // 16x16
            else if (intel_get_accelerator_mb_block_type(accelerator) == 0x0) {
                // One 1st work is needed.
                if (get_local_id(0) == 0) {
                    int index = gid_0 + gid_1 * width;

                    // 1. 1 work-item enabled.
                    // 2. Work-item gathers fwd MV in dword location 0 with constant
                    // offset 8 from
                    //    SLM into short2 locations 0 of global buffer
                    //    search_motion_vector_buffer.
                    // 3. Work-item gathers ushort location 0 from distSearch into ushort
                    //    location 0 of search_residuals with offset index.

                    short2 val = as_short2(dstSearch[8]);
                    search_motion_vector_buffer[index] = val;

#ifndef HW_NULL_CHECK
                    if (search_residuals != NULL)
#endif
                    {
                        search_residuals[index] = distSearch[0];
                    }
                }
            }
        }

        // Write out motion skip check result:
        // Result format
        //     Hierarchical row-major layout
        //     i.e. row-major of blocks in MBs, and row-major of 8 sets of
        //     distortions in blocks
        if (countSkipMVs != 0) {
            if (skip_block_type_8x8 == false) {
                // Copy out 4 (1 component) sets of distortion values.

                int index = (gid_0 * 4) + (get_local_id(0)) + (gid_1 * 4 * width);

                if (get_local_id(0) < countSkipMVs) {
                    // 1. Up to 4 work-items are enabled.
                    // 2. The work-item gathers distSkip locations 0, 16*1, .., 16*7 and
                    //    copies them to contiguous skip_residual locations 0, 1, 2, ..,
                    //    7.
                    __local ushort *distSkip = (__local ushort *)&dstSkipIntra[0];
                    skip_residuals[index] = distSkip[get_local_id(0) * 16];
                }
            } else {
                // Copy out 4 (4 component) sets of distortion values.
                int index =
                    (gid_0 * 4 * 4) + (get_local_id(0)) + (gid_1 * 4 * 4 * width);

                if (get_local_id(0) < countSkipMVs * 4) {
                    // 1. Up to 16 work-items are enabled.
                    // 2. The work-item gathers distSkip locations 0, 4*1, .., 4*15 and
                    //    copies them to contiguous skip_residual locations 0, 1, 2, ..,
                    //    15.

                    __local ushort *distSkip = (__local ushort *)&dstSkipIntra[0];
                    skip_residuals[index] = distSkip[get_local_id(0) * 4];
                }
            }
        }

        // Write out intra search result:
        if (doIntra) {
            // Write out the 4x4 intra modes
            if (get_local_id(0) < 8) {
                __local char *dstIntra_4x4 =
                    (__local char *)(&dstSkipIntra[32 + 16 + 4]);
                char value = dstIntra_4x4[get_local_id(0)];
                char value_low = (value)&0xf;
                char value_high = (value >> 4) & 0xf;

                int index_low =
                    (gid_0 * 22) + (get_local_id(0) * 2) + (gid_1 * 22 * width);

                int index_high =
                    (gid_0 * 22) + (get_local_id(0) * 2) + 1 + (gid_1 * 22 * width);

                intra_search_predictor_modes[index_low + 5] = value_low;
                intra_search_predictor_modes[index_high + 5] = value_high;
            }

            // Write out the 8x8 intra modes
            if (get_local_id(0) < 4) {
                __local char *dstIntra_8x8 =
                    (__local char *)(&dstSkipIntra[32 + 8 + 4]);
                char value = dstIntra_8x8[get_local_id(0) * 2];
                char value_low = (value)&0xf;
                int index = (gid_0 * 22) + (get_local_id(0)) + (gid_1 * 22 * width);
                intra_search_predictor_modes[index + 1] = value_low;
            }

            // Write out the 16x16 intra modes
            if (get_local_id(0) < 1) {
                __local char *dstIntra_16x16 =
                    (__local char *)(&dstSkipIntra[32 + 0 + 4]);
                char value = dstIntra_16x16[0];
                char value_low = (value)&0xf;
                int index = (gid_0 * 22) + (gid_1 * 22 * width);
                intra_search_predictor_modes[index] = value_low;
            }

// Get the intra residuals.
#ifndef HW_NULL_CHECK
            if (intra_residuals != NULL)
#endif
            {
                int index = (gid_0 * 4) + (gid_1 * 4 * width);

                if (get_local_id(0) < 1) {
                    __local ushort *distIntra_4x4 =
                        (__local ushort *)(&dstSkipIntra[32 + 16 + 3]);
                    __local ushort *distIntra_8x8 =
                        (__local ushort *)(&dstSkipIntra[32 + 8 + 3]);
                    __local ushort *distIntra_16x16 =
                        (__local ushort *)(&dstSkipIntra[32 + 0 + 3]);

                    intra_residuals[index + 2] = distIntra_4x4[0];
                    intra_residuals[index + 1] = distIntra_8x8[0];
                    intra_residuals[index + 0] = distIntra_16x16[0];
                }
            }
        }
    }
}

// VEBOX KERNELS:
__kernel void ve_enhance_intel(sampler_t accelerator,
                               int flags,
                               __read_only image2d_t current_input,
                               __write_only image2d_t current_output) {
}

__kernel void ve_dn_enhance_intel(sampler_t accelerator,
                                  int flags,
                                  __read_only image2d_t ref_input,
                                  __read_only image2d_t current_input,
                                  __write_only image2d_t current_output) {
}

__kernel void ve_dn_di_enhance_intel(sampler_t accelerator,
                                     int flags,
                                     __read_only image2d_t current_input,
                                     __read_only image2d_t ref_input,
                                     __write_only image2d_t current_output,
                                     __write_only image2d_t ref_output,
                                     __write_only image2d_t dndi_output) {
}
