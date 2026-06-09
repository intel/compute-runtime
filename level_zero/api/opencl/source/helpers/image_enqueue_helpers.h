/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl.h"

#include <cstddef>
#include <cstdint>

// The shared built-in image<->buffer copy kernels address the host buffer using the row-pitch
// kernel argument for the work-item Y dimension. For a 1D image array the Y dimension is the
// array-layer index, so the distance between consecutive array elements (the slice pitch) must be
// supplied as the row pitch passed to the L0 copy entrypoints. This matters for mip-mapped 1D
// arrays, where the per-element slice pitch (spanning the whole mip chain) is larger than the row
// pitch. Mirrors the legacy OpenCL path (see enqueueReadImage / enqueueWriteImage). For every other
// image type, and when no distinct slice pitch is supplied, the row pitch is used unchanged.
inline uint32_t getL0ImageRowPitch(cl_mem_object_type imageType, size_t rowPitch, size_t slicePitch) {
    if (imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY && slicePitch > rowPitch) {
        return static_cast<uint32_t>(slicePitch);
    }
    return static_cast<uint32_t>(rowPitch);
}
