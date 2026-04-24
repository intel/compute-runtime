/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include "shared/source/memory_manager/memory_manager.h"

namespace L0 {
namespace ult {

Mock<::L0::KernelImp>::Mock() : BaseClass() {
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

    descriptor.kernelAttributes.simdSize = 8;
    descriptor.kernelAttributes.numGrfRequired = 128;
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
