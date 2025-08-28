/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace L0 {
namespace ult {

Mock<::L0::KernelImp>::Mock() : BaseClass() {
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;

    iOpenCL::SPatchExecutionEnvironment execEnv = {};
    execEnv.NumGRFRequired = 128;
    execEnv.LargestCompiledSIMDSize = 8;
    kernelTokens.tokens.executionEnvironment = &execEnv;

    this->sharedState->kernelImmData = &immutableData;

    auto allocation = new NEO::GraphicsAllocation(0,
                                                  1u /*num gmms*/,
                                                  NEO::AllocationType::kernelIsa,
                                                  nullptr,
                                                  0,
                                                  0,
                                                  4096,
                                                  NEO::MemoryPool::system4KBPages,
                                                  NEO::MemoryManager::maxOsContextCount);

    immutableData.isaGraphicsAllocation.reset(allocation);

    NEO::populateKernelDescriptor(descriptor, kernelTokens, 8);
    immutableData.kernelDescriptor = &descriptor;
    immutableData.kernelInfo = &info;
    privateState.crossThreadData.clear();
    privateState.crossThreadData.resize(100U, 0x0);

    privateState.groupSize[0] = 1;
    privateState.groupSize[1] = 1;
    privateState.groupSize[2] = 1;
}
Mock<::L0::KernelImp>::~Mock() {
    delete immutableData.isaGraphicsAllocation.release();
}

} // namespace ult
} // namespace L0
