/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/test/common/helpers/unit_test_helper.h"

namespace NEO {

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isL3ConfigProgrammable() {
    return true;
};

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::evaluateDshUsage(size_t sizeBeforeEnqueue, size_t sizeAfterEnqueue, const KernelDescriptor *kernelDescriptor, uint32_t rootDeviceIndex) {
    if (sizeBeforeEnqueue != sizeAfterEnqueue) {
        return true;
    }
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isTimestampPacketWriteSupported() {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isExpectMemoryNotEqualSupported() {
    return false;
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getDefaultSshUsage() {
    return sizeof(typename GfxFamily::RENDER_SURFACE_STATE);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isAdditionalMiSemaphoreWait(const typename GfxFamily::MI_SEMAPHORE_WAIT &semaphoreWait) {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::evaluateGshAddressForScratchSpace(uint64_t usedScratchGpuAddress, uint64_t retrievedGshAddress) {
    return usedScratchGpuAddress == retrievedGshAddress;
}

template <typename GfxFamily>
auto UnitTestHelper<GfxFamily>::getCoherencyTypeSupported(COHERENCY_TYPE coherencyType) -> decltype(coherencyType) {
    return coherencyType;
}

template <typename GfxFamily>
inline bool UnitTestHelper<GfxFamily>::getPipeControlHdcPipelineFlush(const typename GfxFamily::PIPE_CONTROL &pipeControl) {
    return false;
}

template <typename GfxFamily>
inline void UnitTestHelper<GfxFamily>::setPipeControlHdcPipelineFlush(typename GfxFamily::PIPE_CONTROL &pipeControl, bool hdcPipelineFlush) {}

template <typename GfxFamily>
inline void UnitTestHelper<GfxFamily>::adjustKernelDescriptorForImplicitArgs(KernelDescriptor &kernelDescriptor) {
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer = 0u;
}
} // namespace NEO
