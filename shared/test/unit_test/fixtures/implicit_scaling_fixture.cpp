/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/fixtures/implicit_scaling_fixture.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

void ImplicitScalingFixture::setUp() {
    CommandEncodeStatesFixture::setUp();
    apiSupportBackup = std::make_unique<VariableBackup<bool>>(&ImplicitScaling::apiSupport, true);

    singleTile = DeviceBitfield(static_cast<uint32_t>(maxNBitValue(1)));
    twoTile = DeviceBitfield(static_cast<uint32_t>(maxNBitValue(2)));

    alignedMemory = alignedMalloc(bufferSize, 4096);

    auto gmmHelper = pDevice->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(gpuVa);
    cmdBufferAlloc.setCpuPtrAndGpuAddress(alignedMemory, canonizedGpuAddress);

    commandStream.replaceBuffer(alignedMemory, bufferSize);
    commandStream.replaceGraphicsAllocation(&cmdBufferAlloc);

    testHardwareInfo = *defaultHwInfo;

    dcFlushFlag = pDevice->getDefaultEngine().commandStreamReceiver->getDcFlushSupport();
}

void ImplicitScalingFixture::tearDown() {
    alignedFree(alignedMemory);
    CommandEncodeStatesFixture::tearDown();
}

ImplicitScalingDispatchCommandArgs ImplicitScalingFixture::createDispatchCommandArgs(uint64_t workPartitionAllocationAddress, uint32_t partitionCount) {
    ImplicitScalingDispatchCommandArgs args{
        workPartitionAllocationAddress,  // workPartitionAllocationGpuVa
        this->pDevice,                   // device
        nullptr,                         // outWalkerPtr
        NEO::RequiredPartitionDim::none, // requiredPartitionDim
        partitionCount,                  // partitionCount
        1,                               // workgroupSize
        1,                               // threadGroupCount
        1,                               // maxWgCountPerTile
        true,                            // useSecondaryBatchBuffer
        false,                           // apiSelfCleanup
        dcFlushFlag,                     // dcFlush
        forceExecutionOnSingleTileFlag,  // forceExecutionOnSingleTile
        false,                           // blockDispatchToCommandBuffer
        false};                          // isRequiredDispatchWorkGroupOrder

    return args;
}
