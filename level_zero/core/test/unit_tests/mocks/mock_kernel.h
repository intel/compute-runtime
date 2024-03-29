/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::KernelImmutableData> : public ::L0::KernelImmutableData {
    using BaseClass = ::L0::KernelImmutableData;
    using ::L0::KernelImmutableData::createRelocatedDebugData;
    using ::L0::KernelImmutableData::crossThreadDataSize;
    using ::L0::KernelImmutableData::crossThreadDataTemplate;
    using ::L0::KernelImmutableData::device;
    using ::L0::KernelImmutableData::isaGraphicsAllocation;
    using ::L0::KernelImmutableData::kernelDescriptor;
    using ::L0::KernelImmutableData::KernelImmutableData;
    using ::L0::KernelImmutableData::kernelInfo;
    using ::L0::KernelImmutableData::residencyContainer;
    using ::L0::KernelImmutableData::surfaceStateHeapSize;
    using ::L0::KernelImmutableData::surfaceStateHeapTemplate;

    WhiteBox() : ::L0::KernelImmutableData() {}
};

template <>
struct WhiteBox<::L0::KernelImp> : public ::L0::KernelImp {
    using BaseClass = ::L0::KernelImp;
    using BaseClass::BaseClass;
    using ::L0::KernelImp::createPrintfBuffer;
    using ::L0::KernelImp::crossThreadData;
    using ::L0::KernelImp::crossThreadDataSize;
    using ::L0::KernelImp::dynamicStateHeapData;
    using ::L0::KernelImp::dynamicStateHeapDataSize;
    using ::L0::KernelImp::groupSize;
    using ::L0::KernelImp::isBindlessOffsetSet;
    using ::L0::KernelImp::kernelHasIndirectAccess;
    using ::L0::KernelImp::kernelImmData;
    using ::L0::KernelImp::kernelRequiresGenerationOfLocalIdsByRuntime;
    using ::L0::KernelImp::midThreadPreemptionDisallowedForRayTracingKernels;
    using ::L0::KernelImp::module;
    using ::L0::KernelImp::numThreadsPerThreadGroup;
    using ::L0::KernelImp::patchBindlessOffsetsInCrossThreadData;
    using ::L0::KernelImp::patchBindlessSurfaceState;
    using ::L0::KernelImp::patchSamplerBindlessOffsetsInCrossThreadData;
    using ::L0::KernelImp::perThreadDataForWholeThreadGroup;
    using ::L0::KernelImp::perThreadDataSize;
    using ::L0::KernelImp::perThreadDataSizeForWholeThreadGroup;
    using ::L0::KernelImp::pImplicitArgs;
    using ::L0::KernelImp::printfBuffer;
    using ::L0::KernelImp::requiredWorkgroupOrder;
    using ::L0::KernelImp::residencyContainer;
    using ::L0::KernelImp::setAssertBuffer;
    using ::L0::KernelImp::slmArgsTotalSize;
    using ::L0::KernelImp::suggestGroupSizeCache;
    using ::L0::KernelImp::surfaceStateHeapData;
    using ::L0::KernelImp::surfaceStateHeapDataSize;
    using ::L0::KernelImp::unifiedMemoryControls;
    using ::L0::KernelImp::usingSurfaceStateHeap;

    void setBufferSurfaceState(uint32_t argIndex, void *address,
                               NEO::GraphicsAllocation *alloc) override {}

    void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {}

    WhiteBox() : ::L0::KernelImp(nullptr) {}
};

template <>
struct Mock<::L0::KernelImp> : public WhiteBox<::L0::KernelImp> {
    using BaseClass = WhiteBox<::L0::KernelImp>;
    ADDMETHOD_NOBASE(getProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_kernel_properties_t * pKernelProperties))

    ADDMETHOD(setArgRedescribedImage, ze_result_t, true, ZE_RESULT_SUCCESS,
              (uint32_t argIndex, ze_image_handle_t argVal),
              (argIndex, argVal));

    Mock();
    ~Mock() override;

    void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {}
    void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {
        if (enableForcingOfGenerateLocalIdByHw) {
            kernelRequiresGenerationOfLocalIdsByRuntime = !forceGenerateLocalIdByHw;
        }
    }
    ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation, NEO::SvmAllocationData *peerAllocData) override {
        return ZE_RESULT_SUCCESS;
    }

    void printPrintfOutput(bool hangDetected) override {
        hangDetectedPassedToPrintfOutput = hangDetected;
        printPrintfOutputCalledTimes++;
    }

    WhiteBox<::L0::KernelImmutableData> immutableData;
    NEO::KernelDescriptor descriptor;
    NEO::KernelInfo info;
    uint32_t printPrintfOutputCalledTimes = 0;
    bool hangDetectedPassedToPrintfOutput = false;
    bool enableForcingOfGenerateLocalIdByHw = false;
    bool forceGenerateLocalIdByHw = false;
};

} // namespace ult
} // namespace L0
