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

MemoryPropertiesFlags MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(cl_mem_flags flags, cl_mem_flags_intel flagsIntel, cl_mem_alloc_flags_intel allocflags) {
    MemoryPropertiesFlags memoryPropertiesFlags;

    if (isValueSet(flags, CL_MEM_READ_WRITE)) {
        memoryPropertiesFlags.flags.readWrite = true;
    }
    if (isValueSet(flags, CL_MEM_WRITE_ONLY)) {
        memoryPropertiesFlags.flags.writeOnly = true;
    }
    if (isValueSet(flags, CL_MEM_READ_ONLY)) {
        memoryPropertiesFlags.flags.readOnly = true;
    }
    if (isValueSet(flags, CL_MEM_USE_HOST_PTR)) {
        memoryPropertiesFlags.flags.useHostPtr = true;
    }
    if (isValueSet(flags, CL_MEM_ALLOC_HOST_PTR)) {
        memoryPropertiesFlags.flags.allocHostPtr = true;
    }
    if (isValueSet(flags, CL_MEM_COPY_HOST_PTR)) {
        memoryPropertiesFlags.flags.copyHostPtr = true;
    }
    if (isValueSet(flags, CL_MEM_HOST_WRITE_ONLY)) {
        memoryPropertiesFlags.flags.hostWriteOnly = true;
    }
    if (isValueSet(flags, CL_MEM_HOST_READ_ONLY)) {
        memoryPropertiesFlags.flags.hostReadOnly = true;
    }
    if (isValueSet(flags, CL_MEM_HOST_NO_ACCESS)) {
        memoryPropertiesFlags.flags.hostNoAccess = true;
    }
    if (isValueSet(flags, CL_MEM_KERNEL_READ_AND_WRITE)) {
        memoryPropertiesFlags.flags.kernelReadAndWrite = true;
    }
    if (isValueSet(flags, CL_MEM_FORCE_LINEAR_STORAGE_INTEL) ||
        isValueSet(flagsIntel, CL_MEM_FORCE_LINEAR_STORAGE_INTEL)) {
        memoryPropertiesFlags.flags.forceLinearStorage = true;
    }
    if (isValueSet(flags, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)) {
        memoryPropertiesFlags.flags.accessFlagsUnrestricted = true;
    }
    if (isValueSet(flags, CL_MEM_NO_ACCESS_INTEL)) {
        memoryPropertiesFlags.flags.noAccess = true;
    }
    if (isValueSet(flags, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL) ||
        isValueSet(flagsIntel, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL)) {
        memoryPropertiesFlags.flags.allowUnrestrictedSize = true;
    }
    if (isValueSet(flagsIntel, CL_MEM_LOCALLY_UNCACHED_RESOURCE)) {
        memoryPropertiesFlags.flags.locallyUncachedResource = true;
    }

    if (isValueSet(flagsIntel, CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE)) {
        memoryPropertiesFlags.flags.locallyUncachedInSurfaceState = true;
    }

    if (isValueSet(flags, CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL)) {
        memoryPropertiesFlags.flags.forceSharedPhysicalMemory = true;
    }

    if (isValueSet(allocflags, CL_MEM_ALLOC_WRITE_COMBINED_INTEL)) {
        memoryPropertiesFlags.allocFlags.allocWriteCombined = true;
    }

    if (isValueSet(flagsIntel, CL_MEM_48BIT_RESOURCE_INTEL)) {
        memoryPropertiesFlags.flags.resource48Bit = true;
    }

    addExtraMemoryPropertiesFlags(memoryPropertiesFlags, flags, flagsIntel);

    return memoryPropertiesFlags;
}

} // namespace NEO
