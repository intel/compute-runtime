/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"

namespace NEO {
bool Context::BufferPoolAllocator::flagsAllowBufferFromPool(const cl_mem_flags &flags, const cl_mem_flags_intel &flagsIntel) const {
    return (flagsIntel & CL_MEM_COMPRESSED_HINT_INTEL) == false &&
           (flags & CL_MEM_COMPRESSED_HINT_INTEL) == false;
}

} // namespace NEO