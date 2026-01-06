/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

struct MutableHwCommandFixture : public DeviceFixture {
    void setUp();
    void tearDown();

    template <typename FamilyType, typename WalkerType>
    void createDefaultMutableWalker(void *srcCmdBuffer, bool initCpuPart, bool initGpuPart) {
        this->walkerCmdSize = sizeof(WalkerType);
        this->cmdBufferCpuPtr = L0::MCL::MutableComputeWalkerHw<FamilyType>::createCommandBuffer();

        if (initGpuPart) {
            memcpy(this->cmdBufferGpuPtr, srcCmdBuffer, this->walkerCmdSize);
        }
        if (initCpuPart) {
            memcpy(this->cmdBufferCpuPtr, srcCmdBuffer, this->walkerCmdSize);
        }

        mutableWalker = std::make_unique<L0::MCL::MutableComputeWalkerHw<FamilyType>>(this->cmdBufferGpuPtr,
                                                                                      this->indirectOffset,
                                                                                      this->scratchOffset,
                                                                                      this->cmdBufferCpuPtr,
                                                                                      this->stageCommit);
    }

    template <typename WalkerType>
    void fillWalkerFields(void *walkerCmdBuffer) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(walkerCmdBuffer);

        walkerCmd->setThreadGroupIdXDimension(this->groupCountDimensions[0]);
        walkerCmd->setThreadGroupIdYDimension(this->groupCountDimensions[1]);
        walkerCmd->setThreadGroupIdZDimension(this->groupCountDimensions[2]);
        walkerCmd->getInterfaceDescriptor().setNumberOfThreadsInGpgpuThreadGroup(this->threadsPerThreadGroup);
    }

    L0::MCL::MutableWalkerSpecificFieldsArguments createMutableWalkerSpecificFieldsArguments() {
        return {
            groupCountDimensions,
            groupCountDimensions[0] * groupCountDimensions[1] * groupCountDimensions[2],
            0,
            grfCount,
            threadsPerThreadGroup,
            totalWorkGroupSize,
            slmTotalSize,
            0,
            1,
            8,
            NEO::RequiredPartitionDim::none,
            false,
            false,
            false,
            false,
            false,
            false};
    }

    NEO::LinearStream commandStream;
    std::unique_ptr<NEO::MockGraphicsAllocation> mockAllocation;
    std::unique_ptr<uint64_t[]> bufferStorage;
    std::unique_ptr<L0::MCL::MutableComputeWalker> mutableWalker;
    void *cmdBufferCpuPtr = nullptr;
    void *cmdBufferGpuPtr = nullptr;

    size_t walkerCmdSize = 0;

    uint32_t groupCountDimensions[3] = {2, 2, 2};
    uint32_t grfCount = 128;
    uint32_t simdSize = 32;
    uint32_t threadsPerThreadGroup = 2;
    uint32_t totalWorkGroupSize = threadsPerThreadGroup * simdSize;
    uint32_t slmTotalSize = 0;

    uint8_t indirectOffset = 0;
    uint8_t scratchOffset = 8;

    bool stageCommit = true;
    bool isHeapless = false;
};

} // namespace ult
} // namespace L0
