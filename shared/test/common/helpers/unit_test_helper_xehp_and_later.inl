/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/debug_settings/debug_settings_manager.h"

#include "opencl/source/kernel/kernel.h"

namespace NEO {

template <>
bool UnitTestHelper<Family>::isL3ConfigProgrammable() {
    return false;
};

template <>
bool UnitTestHelper<Family>::evaluateDshUsage(size_t sizeBeforeEnqueue, size_t sizeAfterEnqueue, const KernelDescriptor *kernelDescriptor, uint32_t rootDeviceIndex) {
    if (kernelDescriptor == nullptr) {
        if (sizeBeforeEnqueue == sizeAfterEnqueue) {
            return true;
        }
        return false;
    }

    auto samplerCount = kernelDescriptor->payloadMappings.samplerTable.numSamplers;
    if (samplerCount > 0) {
        if (sizeBeforeEnqueue != sizeAfterEnqueue) {
            return true;
        }
        return false;
    } else {
        if (sizeBeforeEnqueue == sizeAfterEnqueue) {
            return true;
        }
        return false;
    }
}

template <>
bool UnitTestHelper<Family>::isTimestampPacketWriteSupported() {
    return true;
};

template <>
bool UnitTestHelper<Family>::isExpectMemoryNotEqualSupported() {
    return true;
}

template <>
uint32_t UnitTestHelper<Family>::getDefaultSshUsage() {
    return (32 * 2 * 64);
}

template <>
bool UnitTestHelper<Family>::isAdditionalMiSemaphoreWait(const typename Family::MI_SEMAPHORE_WAIT &semaphoreWait) {
    return (semaphoreWait.getSemaphoreDataDword() == EncodeSempahore<Family>::invalidHardwareTag);
}

template <>
bool UnitTestHelper<Family>::evaluateGshAddressForScratchSpace(uint64_t usedScratchGpuAddress, uint64_t retrievedGshAddress) {
    return 0llu == retrievedGshAddress;
}

template <>
auto UnitTestHelper<Family>::getCoherencyTypeSupported(COHERENCY_TYPE coherencyType) -> decltype(coherencyType) {
    return Family::RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT;
}

} // namespace NEO
