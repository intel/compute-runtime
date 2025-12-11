/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"

namespace NEO {
bool Context::BufferPoolAllocator::flagsAllowBufferFromPool(const cl_mem_flags &flags, const cl_mem_flags_intel &flagsIntel) const {
    auto forbiddenFlag = this->poolType == BufferPoolType::SmallBuffersPool ? CL_MEM_COMPRESSED_HINT_INTEL : CL_MEM_UNCOMPRESSED_HINT_INTEL;
    return (flagsIntel & forbiddenFlag) == false &&
           (flags & forbiddenFlag) == false;
}

} // namespace NEO