/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"

#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::KernelImmutableData> : public ::L0::KernelImmutableData {
    using BaseClass = ::L0::KernelImmutableData;
    using ::L0::KernelImmutableData::crossThreadDataSize;
    using ::L0::KernelImmutableData::crossThreadDataTemplate;
    using ::L0::KernelImmutableData::device;
    using ::L0::KernelImmutableData::isaGraphicsAllocation;
    using ::L0::KernelImmutableData::kernelDescriptor;
    using ::L0::KernelImmutableData::KernelImmutableData;
    using ::L0::KernelImmutableData::privateMemoryGraphicsAllocation;
    using ::L0::KernelImmutableData::residencyContainer;

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
    using ::L0::KernelImp::perThreadDataForWholeThreadGroup;
    using ::L0::KernelImp::perThreadDataSize;
    using ::L0::KernelImp::perThreadDataSizeForWholeThreadGroup;
    using ::L0::KernelImp::printfBuffer;
    using ::L0::KernelImp::requiredWorkgroupOrder;
    using ::L0::KernelImp::residencyContainer;
    using ::L0::KernelImp::unifiedMemoryControls;

    void setBufferSurfaceState(uint32_t argIndex, void *address,
                               NEO::GraphicsAllocation *alloc) override {}

    void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {}

    std::unique_ptr<Kernel> clone() const override { return nullptr; }

    WhiteBox() : ::L0::KernelImp(nullptr) {}
};

template <>
struct Mock<::L0::Kernel> : public WhiteBox<::L0::Kernel> {
    using BaseClass = WhiteBox<::L0::Kernel>;

    Mock() : BaseClass(nullptr) {
        NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
        iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
        kernelTokens.header = &kernelHeader;

        iOpenCL::SPatchExecutionEnvironment execEnv = {};
        kernelTokens.tokens.executionEnvironment = &execEnv;

        this->kernelImmData = &immutableData;

        auto allocation = new NEO::GraphicsAllocation(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                      nullptr, 0, 0, 4096,
                                                      MemoryPool::System4KBPages);

        immutableData.isaGraphicsAllocation.reset(allocation);

        NEO::populateKernelDescriptor(descriptor, kernelTokens, 8);
        immutableData.kernelDescriptor = &descriptor;
    }
    ~Mock() override {
        delete immutableData.isaGraphicsAllocation.release();
    }

    void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {}
    void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {}
    std::unique_ptr<Kernel> clone() const override {
        return nullptr;
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
