/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/kernel/kernel_hw.h"
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
struct WhiteBox<::L0::Kernel> : public ::L0::KernelImp {
    using BaseClass = ::L0::KernelImp;
    using BaseClass::BaseClass;
    using ::L0::KernelImp::createPrintfBuffer;
    using ::L0::KernelImp::crossThreadData;
    using ::L0::KernelImp::crossThreadDataSize;
    using ::L0::KernelImp::groupSize;
    using ::L0::KernelImp::kernelImmData;
    using ::L0::KernelImp::kernelRequiresGenerationOfLocalIdsByRuntime;
    using ::L0::KernelImp::module;
    using ::L0::KernelImp::numThreadsPerThreadGroup;
    using ::L0::KernelImp::patchBindlessSurfaceState;
    using ::L0::KernelImp::perThreadDataForWholeThreadGroup;
    using ::L0::KernelImp::perThreadDataSize;
    using ::L0::KernelImp::perThreadDataSizeForWholeThreadGroup;
    using ::L0::KernelImp::pImplicitArgs;
    using ::L0::KernelImp::printfBuffer;
    using ::L0::KernelImp::requiredWorkgroupOrder;
    using ::L0::KernelImp::residencyContainer;
    using ::L0::KernelImp::surfaceStateHeapData;
    using ::L0::KernelImp::surfaceStateHeapDataSize;
    using ::L0::KernelImp::unifiedMemoryControls;

    void setBufferSurfaceState(uint32_t argIndex, void *address,
                               NEO::GraphicsAllocation *alloc) override {}

    void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {}

    WhiteBox() : ::L0::KernelImp(nullptr) {}
};
template <GFXCORE_FAMILY gfxCoreFamily>
struct WhiteBoxKernelHw : public KernelHw<gfxCoreFamily> {
    using BaseClass = KernelHw<gfxCoreFamily>;
    using BaseClass::BaseClass;
    using ::L0::KernelImp::createPrintfBuffer;
    using ::L0::KernelImp::crossThreadData;
    using ::L0::KernelImp::crossThreadDataSize;
    using ::L0::KernelImp::groupSize;
    using ::L0::KernelImp::kernelImmData;
    using ::L0::KernelImp::kernelRequiresGenerationOfLocalIdsByRuntime;
    using ::L0::KernelImp::module;
    using ::L0::KernelImp::numThreadsPerThreadGroup;
    using ::L0::KernelImp::patchBindlessSurfaceState;
    using ::L0::KernelImp::perThreadDataForWholeThreadGroup;
    using ::L0::KernelImp::perThreadDataSize;
    using ::L0::KernelImp::perThreadDataSizeForWholeThreadGroup;
    using ::L0::KernelImp::printfBuffer;
    using ::L0::KernelImp::requiredWorkgroupOrder;
    using ::L0::KernelImp::residencyContainer;
    using ::L0::KernelImp::surfaceStateHeapData;
    using ::L0::KernelImp::unifiedMemoryControls;

    void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {}

    WhiteBoxKernelHw() : ::L0::KernelHw<gfxCoreFamily>(nullptr) {}
};

template <>
struct Mock<::L0::Kernel> : public WhiteBox<::L0::Kernel> {
    using BaseClass = WhiteBox<::L0::Kernel>;
    ADDMETHOD_NOBASE(getProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_kernel_properties_t * pKernelProperties))

    ADDMETHOD(setArgRedescribedImage, ze_result_t, true, ZE_RESULT_SUCCESS,
              (uint32_t argIndex, ze_image_handle_t argVal),
              (argIndex, argVal));

    Mock() : BaseClass(nullptr) {
        NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
        iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
        kernelTokens.header = &kernelHeader;

        iOpenCL::SPatchExecutionEnvironment execEnv = {};
        execEnv.LargestCompiledSIMDSize = 8;
        kernelTokens.tokens.executionEnvironment = &execEnv;

        this->kernelImmData = &immutableData;

        auto allocation = new NEO::GraphicsAllocation(0,
                                                      NEO::AllocationType::KERNEL_ISA,
                                                      nullptr,
                                                      0,
                                                      0,
                                                      4096,
                                                      NEO::MemoryPool::System4KBPages,
                                                      NEO::MemoryManager::maxOsContextCount);

        immutableData.isaGraphicsAllocation.reset(allocation);

        NEO::populateKernelDescriptor(descriptor, kernelTokens, 8);
        immutableData.kernelDescriptor = &descriptor;
        crossThreadData.reset(new uint8_t[100]);
    }
    ~Mock() override {
        delete immutableData.isaGraphicsAllocation.release();
    }

    void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {}
    void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {}
    ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation) override {
        return ZE_RESULT_SUCCESS;
    }

    void printPrintfOutput() override {
        printPrintfOutputCalledTimes++;
    }

    WhiteBox<::L0::KernelImmutableData> immutableData;
    NEO::KernelDescriptor descriptor;
    uint32_t printPrintfOutputCalledTimes = 0;
};

} // namespace ult
} // namespace L0
