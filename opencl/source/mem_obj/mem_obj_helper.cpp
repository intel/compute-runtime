/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/mem_obj_helper.h"

#include "shared/source/helpers/memory_properties_helpers.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_hw_helper.h"

namespace NEO {

bool MemObjHelper::validateMemoryPropertiesForBuffer(const MemoryProperties &memoryProperties, cl_mem_flags flags,
                                                     cl_mem_flags_intel flagsIntel, const Context &context) {
    /* Check all the invalid flags combination. */
    if ((isValueSet(flags, CL_MEM_READ_WRITE | CL_MEM_READ_ONLY)) ||
        (isValueSet(flags, CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY)) ||
        (isValueSet(flags, CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY)) ||
        (isValueSet(flags, CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR)) ||
        (isValueSet(flags, CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR)) ||
        (isValueSet(flags, CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)) ||
        (isValueSet(flags, CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_WRITE_ONLY)) ||
        (isValueSet(flags, CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS))) {
        return false;
    }

    return validateExtraMemoryProperties(memoryProperties, flags, flagsIntel, context);
}

bool MemObjHelper::validateMemoryPropertiesForImage(const MemoryProperties &memoryProperties, cl_mem_flags flags,
                                                    cl_mem_flags_intel flagsIntel, cl_mem parent, const Context &context) {
    /* Check all the invalid flags combination. */
    if ((!isValueSet(flags, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)) &&
        (isValueSet(flags, CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY) ||
         isValueSet(flags, CL_MEM_READ_WRITE | CL_MEM_READ_ONLY) ||
         isValueSet(flags, CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY) ||
         isValueSet(flags, CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR) ||
         isValueSet(flags, CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR) ||
         isValueSet(flags, CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY) ||
         isValueSet(flags, CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS) ||
         isValueSet(flags, CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS) ||
         isValueSet(flags, CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_WRITE) ||
         isValueSet(flags, CL_MEM_NO_ACCESS_INTEL | CL_MEM_WRITE_ONLY) ||
         isValueSet(flags, CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_ONLY))) {
        return false;
    }

    auto parentMemObj = castToObject<MemObj>(parent);
    if (parentMemObj != nullptr && flags) {
        auto parentFlags = parentMemObj->getFlags();
        /* Check whether flags are compatible with parent. */
        if (isValueSet(flags, CL_MEM_ALLOC_HOST_PTR) ||
            isValueSet(flags, CL_MEM_COPY_HOST_PTR) ||
            isValueSet(flags, CL_MEM_USE_HOST_PTR) ||
            ((!isValueSet(parentFlags, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)) &&
             (!isValueSet(flags, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)) &&
             ((isValueSet(parentFlags, CL_MEM_WRITE_ONLY) && isValueSet(flags, CL_MEM_READ_WRITE)) ||
              (isValueSet(parentFlags, CL_MEM_WRITE_ONLY) && isValueSet(flags, CL_MEM_READ_ONLY)) ||
              (isValueSet(parentFlags, CL_MEM_READ_ONLY) && isValueSet(flags, CL_MEM_READ_WRITE)) ||
              (isValueSet(parentFlags, CL_MEM_READ_ONLY) && isValueSet(flags, CL_MEM_WRITE_ONLY)) ||
              (isValueSet(parentFlags, CL_MEM_NO_ACCESS_INTEL) && isValueSet(flags, CL_MEM_READ_WRITE)) ||
              (isValueSet(parentFlags, CL_MEM_NO_ACCESS_INTEL) && isValueSet(flags, CL_MEM_WRITE_ONLY)) ||
              (isValueSet(parentFlags, CL_MEM_NO_ACCESS_INTEL) && isValueSet(flags, CL_MEM_READ_ONLY)) ||
              (isValueSet(parentFlags, CL_MEM_HOST_NO_ACCESS) && isValueSet(flags, CL_MEM_HOST_WRITE_ONLY)) ||
              (isValueSet(parentFlags, CL_MEM_HOST_NO_ACCESS) && isValueSet(flags, CL_MEM_HOST_READ_ONLY))))) {
            return false;
        }
    }

    return validateExtraMemoryProperties(memoryProperties, flags, flagsIntel, context);
}

AllocationProperties MemObjHelper::getAllocationPropertiesWithImageInfo(
    uint32_t rootDeviceIndex, ImageInfo &imgInfo, bool allocateMemory, const MemoryProperties &memoryProperties,
    const HardwareInfo &hwInfo, DeviceBitfield subDevicesBitfieldParam, bool deviceOnlyVisibilty) {

    auto deviceBitfield = MemoryPropertiesHelper::adjustDeviceBitfield(rootDeviceIndex, memoryProperties, subDevicesBitfieldParam);
    AllocationProperties allocationProperties{rootDeviceIndex, allocateMemory, imgInfo, AllocationType::IMAGE, deviceBitfield};
    MemoryPropertiesHelper::fillPoliciesInProperties(allocationProperties, memoryProperties, hwInfo, deviceOnlyVisibilty);
    return allocationProperties;
}

bool MemObjHelper::checkMemFlagsForSubBuffer(cl_mem_flags flags) {
    const cl_mem_flags allValidFlags =
        CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
        CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

    return isFieldValid(flags, allValidFlags);
}

SVMAllocsManager::SvmAllocationProperties MemObjHelper::getSvmAllocationProperties(cl_mem_flags flags) {
    SVMAllocsManager::SvmAllocationProperties svmProperties;
    svmProperties.coherent = isValueSet(flags, CL_MEM_SVM_FINE_GRAIN_BUFFER);
    svmProperties.hostPtrReadOnly = isValueSet(flags, CL_MEM_HOST_READ_ONLY) || isValueSet(flags, CL_MEM_HOST_NO_ACCESS);
    svmProperties.readOnly = isValueSet(flags, CL_MEM_READ_ONLY);
    return svmProperties;
}

bool MemObjHelper::isSuitableForCompression(bool compressionSupported, const MemoryProperties &properties, Context &context, bool preferCompression) {
    if (!compressionSupported) {
        return false;
    }
    if (context.getRootDeviceIndices().size() > 1u) {
        return false;
    }
    for (auto &pClDevice : context.getDevices()) {
        auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
        auto &hwInfo = pClDevice->getHardwareInfo();
        auto &clHwHelper = ClHwHelper::get(hwInfo.platform.eRenderCoreFamily);
        if (!clHwHelper.allowCompressionForContext(*pClDevice, context)) {
            return false;
        }

        if (context.containsMultipleSubDevices(rootDeviceIndex)) {
            if (DebugManager.flags.EnableMultiTileCompression.get() <= 0) {
                return false;
            }

            //for unrestrictive and default context, turn on compression only for read only surfaces with no host access.
            bool isContextSpecialized = (context.peekContextType() == ContextType::CONTEXT_TYPE_SPECIALIZED);
            bool isReadOnlyAndHostNoAccess = (properties.flags.readOnly && properties.flags.hostNoAccess);
            if (!isContextSpecialized && !isReadOnlyAndHostNoAccess) {
                return false;
            }
        }
    }
    if (preferCompression) {
        if (properties.flags.uncompressedHint) {
            return false;
        }
        if (properties.flags.compressedHint) {
            return true;
        }
        int32_t disableCompression = DebugManager.flags.ToggleHintKernelDisableCompression.get();
        if (disableCompression != -1) {
            return !!disableCompression;
        } else {
            if (context.getResolvesRequiredInKernels()) {
                return false;
            }
        }
        return true;
    }

    return properties.flags.compressedHint;
}

bool MemObjHelper::validateExtraMemoryProperties(const MemoryProperties &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel, const Context &context) {
    bool compressedFlagSet = isValueSet(flags, CL_MEM_COMPRESSED_HINT_INTEL) ||
                             isValueSet(flagsIntel, CL_MEM_COMPRESSED_HINT_INTEL);
    bool uncompressedFlagSet = isValueSet(flags, CL_MEM_UNCOMPRESSED_HINT_INTEL) ||
                               isValueSet(flagsIntel, CL_MEM_UNCOMPRESSED_HINT_INTEL);
    if (compressedFlagSet && uncompressedFlagSet) {
        return false;
    }

    auto pClDevice = memoryProperties.pDevice->getSpecializedDevice<ClDevice>();
    auto &contextRootDeviceIndices = context.getRootDeviceIndices();
    bool isRootDeviceAssociated = (std::find(contextRootDeviceIndices.begin(), contextRootDeviceIndices.end(), pClDevice->getRootDeviceIndex()) != contextRootDeviceIndices.end());
    return isRootDeviceAssociated;
}

const uint64_t MemObjHelper::commonFlags = CL_MEM_COMPRESSED_HINT_INTEL | CL_MEM_UNCOMPRESSED_HINT_INTEL | CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
                                           CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR |
                                           CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

const uint64_t MemObjHelper::commonFlagsIntel = CL_MEM_COMPRESSED_HINT_INTEL | CL_MEM_UNCOMPRESSED_HINT_INTEL | CL_MEM_LOCALLY_UNCACHED_RESOURCE |
                                                CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE | CL_MEM_48BIT_RESOURCE_INTEL;

const uint64_t MemObjHelper::validFlagsForBuffer = commonFlags | CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL | CL_MEM_FORCE_HOST_MEMORY_INTEL;

const uint64_t MemObjHelper::validFlagsForBufferIntel = commonFlagsIntel | CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;

const uint64_t MemObjHelper::validFlagsForImage = commonFlags | CL_MEM_NO_ACCESS_INTEL | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_FORCE_LINEAR_STORAGE_INTEL;

const uint64_t MemObjHelper::validFlagsForImageIntel = commonFlagsIntel;

} // namespace NEO
