/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bit_helpers.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/helpers/memory_properties_flags_helpers.h"

#include "CL/cl_ext_intel.h"

namespace NEO {

MemoryProperties MemoryPropertiesParser::createMemoryProperties(cl_mem_flags flags, cl_mem_flags_intel flagsIntel, cl_mem_alloc_flags_intel allocflags) {
    MemoryProperties memoryProperties;

    if (isValueSet(flags, CL_MEM_READ_WRITE)) {
        memoryProperties.flags.readWrite = true;
    }
    if (isValueSet(flags, CL_MEM_WRITE_ONLY)) {
        memoryProperties.flags.writeOnly = true;
    }
    if (isValueSet(flags, CL_MEM_READ_ONLY)) {
        memoryProperties.flags.readOnly = true;
    }
    if (isValueSet(flags, CL_MEM_USE_HOST_PTR)) {
        memoryProperties.flags.useHostPtr = true;
    }
    if (isValueSet(flags, CL_MEM_ALLOC_HOST_PTR)) {
        memoryProperties.flags.allocHostPtr = true;
    }
    if (isValueSet(flags, CL_MEM_COPY_HOST_PTR)) {
        memoryProperties.flags.copyHostPtr = true;
    }
    if (isValueSet(flags, CL_MEM_HOST_WRITE_ONLY)) {
        memoryProperties.flags.hostWriteOnly = true;
    }
    if (isValueSet(flags, CL_MEM_HOST_READ_ONLY)) {
        memoryProperties.flags.hostReadOnly = true;
    }
    if (isValueSet(flags, CL_MEM_HOST_NO_ACCESS)) {
        memoryProperties.flags.hostNoAccess = true;
    }
    if (isValueSet(flags, CL_MEM_KERNEL_READ_AND_WRITE)) {
        memoryProperties.flags.kernelReadAndWrite = true;
    }
    if (isValueSet(flags, CL_MEM_FORCE_LINEAR_STORAGE_INTEL) ||
        isValueSet(flagsIntel, CL_MEM_FORCE_LINEAR_STORAGE_INTEL)) {
        memoryProperties.flags.forceLinearStorage = true;
    }
    if (isValueSet(flags, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)) {
        memoryProperties.flags.accessFlagsUnrestricted = true;
    }
    if (isValueSet(flags, CL_MEM_NO_ACCESS_INTEL)) {
        memoryProperties.flags.noAccess = true;
    }
    if (isValueSet(flags, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL) ||
        isValueSet(flagsIntel, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL)) {
        memoryProperties.flags.allowUnrestrictedSize = true;
    }
    if (isValueSet(flagsIntel, CL_MEM_LOCALLY_UNCACHED_RESOURCE)) {
        memoryProperties.flags.locallyUncachedResource = true;
    }

    if (isValueSet(flagsIntel, CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE)) {
        memoryProperties.flags.locallyUncachedInSurfaceState = true;
    }

    if (isValueSet(flags, CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL)) {
        memoryProperties.flags.forceSharedPhysicalMemory = true;
    }

    if (isValueSet(allocflags, CL_MEM_ALLOC_WRITE_COMBINED_INTEL)) {
        memoryProperties.allocFlags.allocWriteCombined = true;
    }

    if (isValueSet(flagsIntel, CL_MEM_48BIT_RESOURCE_INTEL)) {
        memoryProperties.flags.resource48Bit = true;
    }

    addExtraMemoryProperties(memoryProperties, flags, flagsIntel);

    return memoryProperties;
}

} // namespace NEO
