/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"

namespace NEO {
bool Context::BufferPoolAllocator::flagsAllowBufferFromPool(const cl_mem_flags &flags, const cl_mem_flags_intel &flagsIntel) const {
    return true;
}

} // namespace NEO