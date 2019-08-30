/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "common/helpers/bit_helpers.h"
#include "public/cl_ext_private.h"
#include "runtime/helpers/memory_properties_flags_helpers.h"

#include "CL/cl_ext_intel.h"

namespace NEO {

MemoryPropertiesFlags MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(MemoryProperties properties) {
    MemoryPropertiesFlags memoryPropertiesFlags;

    if (isValueSet(properties.flags, CL_MEM_READ_WRITE)) {
        memoryPropertiesFlags.flags.readWrite = true;
    }
    if (isValueSet(properties.flags, CL_MEM_WRITE_ONLY)) {
        memoryPropertiesFlags.flags.writeOnly = true;
    }
    if (isValueSet(properties.flags, CL_MEM_READ_ONLY)) {
        memoryPropertiesFlags.flags.readOnly = true;
    }
    if (isValueSet(properties.flags, CL_MEM_USE_HOST_PTR)) {
        memoryPropertiesFlags.flags.useHostPtr = true;
    }
    if (isValueSet(properties.flags, CL_MEM_ALLOC_HOST_PTR)) {
        memoryPropertiesFlags.flags.allocHostPtr = true;
    }
    if (isValueSet(properties.flags, CL_MEM_COPY_HOST_PTR)) {
        memoryPropertiesFlags.flags.copyHostPtr = true;
    }
    if (isValueSet(properties.flags, CL_MEM_HOST_WRITE_ONLY)) {
        memoryPropertiesFlags.flags.hostWriteOnly = true;
    }
    if (isValueSet(properties.flags, CL_MEM_HOST_READ_ONLY)) {
        memoryPropertiesFlags.flags.hostReadOnly = true;
    }
    if (isValueSet(properties.flags, CL_MEM_HOST_NO_ACCESS)) {
        memoryPropertiesFlags.flags.hostNoAccess = true;
    }
    if (isValueSet(properties.flags, CL_MEM_KERNEL_READ_AND_WRITE)) {
        memoryPropertiesFlags.flags.kernelReadAndWrite = true;
    }
    if (isValueSet(properties.flags, CL_MEM_FORCE_LINEAR_STORAGE_INTEL) ||
        isValueSet(properties.flags_intel, CL_MEM_FORCE_LINEAR_STORAGE_INTEL)) {
        memoryPropertiesFlags.flags.forceLinearStorage = true;
    }
    if (isValueSet(properties.flags, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)) {
        memoryPropertiesFlags.flags.accessFlagsUnrestricted = true;
    }
    if (isValueSet(properties.flags, CL_MEM_NO_ACCESS_INTEL)) {
        memoryPropertiesFlags.flags.noAccess = true;
    }
    if (isValueSet(properties.flags, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL) ||
        isValueSet(properties.flags_intel, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL)) {
        memoryPropertiesFlags.flags.allowUnrestrictedSize = true;
    }
    if (isValueSet(properties.flags_intel, CL_MEM_LOCALLY_UNCACHED_RESOURCE)) {
        memoryPropertiesFlags.flags.locallyUncachedResource = true;
    }

    if (isValueSet(properties.flags_intel, CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE)) {
        memoryPropertiesFlags.flags.locallyUncachedInSurfaceState = true;
    }

    if (isValueSet(properties.flags, CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL)) {
        memoryPropertiesFlags.flags.forceSharedPhysicalMemory = true;
    }

    addExtraMemoryPropertiesFlags(memoryPropertiesFlags, properties);

    return memoryPropertiesFlags;
}

} // namespace NEO
