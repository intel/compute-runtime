/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/unified_memory/unified_memory.h"

#include "aubstream/product_family.h"

namespace NEO {

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Bmg;
};

template <>
std::optional<GfxMemoryAllocationMethod> ProductHelperHw<gfxProduct>::getPreferredAllocationMethod(AllocationType allocationType) const {
    if constexpr (is32bit) {
        if (allocationType == AllocationType::svmCpu) { // no heap SVM in allocateByKmd on 32 bit
            return GfxMemoryAllocationMethod::useUmdSystemPtr;
        }
    }
    return GfxMemoryAllocationMethod::allocateByKmd;
}

template <>
void ProductHelperHw<gfxProduct>::adjustNumberOfCcs(HardwareInfo &hwInfo) const {
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
}

template <>
bool ProductHelperHw<gfxProduct>::adjustDispatchAllRequired(const HardwareInfo &hwInfo) const {
    if (hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 1u) {
        return false;
    }
    return hwInfo.gtSystemInfo.SliceCount > 2u;
}

template <>
void ProductHelperHw<gfxProduct>::adjustScratchSize(size_t &requiredScratchSize) const {
    requiredScratchSize *= 2;
}

template <>
bool ProductHelperHw<gfxProduct>::checkBcsForDirectSubmissionStop() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceUsmPoolAllocatorSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isHostUsmPoolAllocatorSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::initializeInternalEngineImmediately() const {
    return false;
}

template <>
size_t ProductHelperHw<gfxProduct>::getCpuCopyThreshold(TransferType transferType) const {
    size_t threshold = 0u;

    switch (transferType) {
    case TransferType::deviceUsmToDeviceUsm:
        threshold = 4 * MemoryConstants::kiloByte;
        break;
    case TransferType::deviceUsmToHostUsm:
        threshold = 4 * MemoryConstants::kiloByte;
        break;
    case TransferType::deviceUsmToHostNonUsm:
        threshold = 64 * MemoryConstants::kiloByte;
        break;
    case TransferType::hostUsmToDeviceUsm:
        threshold = 64 * MemoryConstants::kiloByte;
        break;
    case TransferType::hostUsmToHostUsm:
        threshold = 1 * MemoryConstants::megaByte;
        break;
    case TransferType::hostUsmToHostNonUsm:
        threshold = 64 * MemoryConstants::megaByte;
        break;
    case TransferType::hostNonUsmToDeviceUsm:
        threshold = 10 * MemoryConstants::megaByte;
        break;
    case TransferType::hostNonUsmToHostUsm:
        threshold = 64 * MemoryConstants::megaByte;
        break;
    case TransferType::hostNonUsmToHostNonUsm:
        threshold = 64 * MemoryConstants::megaByte;
        break;
    default:
        break;
    }

    return threshold;
}

} // namespace NEO
