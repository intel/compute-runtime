/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/mem_obj_helper_common.inl"

#include "memory_properties_flags.h"

namespace NEO {

bool MemObjHelper::isSuitableForRenderCompression(bool renderCompressed, const MemoryProperties &properties, Context &context, bool preferCompression) {
    if (context.getRootDeviceIndices().size() > 1u) {
        return false;
    }
    return renderCompressed && preferCompression;
}

bool MemObjHelper::validateExtraMemoryProperties(const MemoryProperties &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel, const Context &context) {
    return true;
}

const uint64_t MemObjHelper::extraFlags = 0;

const uint64_t MemObjHelper::extraFlagsIntel = 0;

} // namespace NEO
