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
        memoryPropertiesFlags.readWrite = true;
    }
    if (isValueSet(properties.flags, CL_MEM_WRITE_ONLY)) {
        memoryPropertiesFlags.writeOnly = true;
    }
    if (isValueSet(properties.flags, CL_MEM_READ_ONLY)) {
        memoryPropertiesFlags.readOnly = true;
    }
    if (isValueSet(properties.flags, CL_MEM_USE_HOST_PTR)) {
        memoryPropertiesFlags.useHostPtr = true;
    }
    if (isValueSet(properties.flags, CL_MEM_ALLOC_HOST_PTR)) {
        memoryPropertiesFlags.allocHostPtr = true;
    }
    if (isValueSet(properties.flags, CL_MEM_COPY_HOST_PTR)) {
        memoryPropertiesFlags.copyHostPtr = true;
    }
    if (isValueSet(properties.flags, CL_MEM_HOST_WRITE_ONLY)) {
        memoryPropertiesFlags.hostWriteOnly = true;
    }
    if (isValueSet(properties.flags, CL_MEM_HOST_READ_ONLY)) {
        memoryPropertiesFlags.hostReadOnly = true;
    }
    if (isValueSet(properties.flags, CL_MEM_HOST_NO_ACCESS)) {
        memoryPropertiesFlags.hostNoAccess = true;
    }
    if (isValueSet(properties.flags, CL_MEM_KERNEL_READ_AND_WRITE)) {
        memoryPropertiesFlags.kernelReadAndWrite = true;
    }
    if (isValueSet(properties.flags, CL_MEM_FORCE_LINEAR_STORAGE_INTEL) ||
        isValueSet(properties.flags_intel, CL_MEM_FORCE_LINEAR_STORAGE_INTEL)) {
        memoryPropertiesFlags.forceLinearStorage = true;
    }
    if (isValueSet(properties.flags, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)) {
        memoryPropertiesFlags.accessFlagsUnrestricted = true;
    }
    if (isValueSet(properties.flags, CL_MEM_NO_ACCESS_INTEL)) {
        memoryPropertiesFlags.noAccess = true;
    }
    if (isValueSet(properties.flags, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL) ||
        isValueSet(properties.flags_intel, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL)) {
        memoryPropertiesFlags.allowUnrestrictedSize = true;
    }
    if (isValueSet(properties.flags_intel, CL_MEM_LOCALLY_UNCACHED_RESOURCE)) {
        memoryPropertiesFlags.locallyUncachedResource = true;
    }

    addExtraMemoryPropertiesFlags(memoryPropertiesFlags, properties);

    return memoryPropertiesFlags;
}

} // namespace NEO