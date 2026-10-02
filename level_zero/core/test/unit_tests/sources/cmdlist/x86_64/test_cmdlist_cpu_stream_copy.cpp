/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/helpers/x86_64/stream_copy_blocks_ult.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/mock_cpuid_functions.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

struct CommandListCpuStreamCopyFixture : public DeviceFixture {
    void setUp() {
        debugManager.flags.ExperimentalCopyThroughLock.set(1);
        debugManager.flags.EnableLocalMemory.set(1);
        DeviceFixture::setUp();

        auto *mockCpuInfo = getMockCpuInfo(NEO::CpuInfo::getInstance());
        featuresBackup = std::make_unique<VariableBackup<uint64_t>>(&mockCpuInfo->features);
        featuresDetectedBackup = std::make_unique<VariableBackup<bool>>(&mockCpuInfo->featuresDetected);
        mockCpuInfo->features = NEO::CpuInfo::featureSse41;
        mockCpuInfo->featuresDetected = true;

        ze_device_mem_alloc_desc_t deviceDesc = {};
        context->allocDeviceMem(device->toHandle(), &deviceDesc, bufferSize, 1u, &devicePtr);
        hostBuffer = allocateAlignedMemory(bufferSize, MemoryConstants::cacheLineSize);
    }

    void tearDown() {
        context->freeMem(devicePtr);
        DeviceFixture::tearDown();
    }

    DebugManagerStateRestore restore;
    std::unique_ptr<VariableBackup<uint64_t>> featuresBackup;
    std::unique_ptr<VariableBackup<bool>> featuresDetectedBackup;
    decltype(allocateAlignedMemory(0u, 0u)) hostBuffer;
    void *devicePtr = nullptr;
    const size_t bufferSize = MemoryConstants::pageSize;
    const size_t copySize = 1024u;
    const size_t srcMisalignment = 1u;
};

using CommandListCpuStreamCopyTest = Test<CommandListCpuStreamCopyFixture>;

HWTEST_F(CommandListCpuStreamCopyTest, givenLockedDeviceSourceWhenPerformCpuMemcpyThenMisalignedSourceHeadIsLoadedAsBlock) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    NEO::SvmAllocationData *allocData = nullptr;
    ASSERT_TRUE(device->getDriverHandle()->findAllocationDataForRange(devicePtr, bufferSize, allocData));
    auto *srcAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    auto *lockedPtr = static_cast<uint8_t *>(device->getDriverHandle()->getMemoryManager()->lockResource(srcAllocation));
    ASSERT_NE(nullptr, lockedPtr);
    ASSERT_TRUE(isAligned<MemoryConstants::cacheLineSize>(lockedPtr));
    for (size_t i = 0; i < bufferSize; ++i) {
        lockedPtr[i] = static_cast<uint8_t>(i);
    }

    CpuMemCopyInfo cpuMemCopyInfo(hostBuffer.get(), ptrOffset(devicePtr, srcMisalignment), copySize);
    cmdList.obtainAllocData(cpuMemCopyInfo, false);

    NEO::StreamCopyBlocksUlt::reset();
    NEO::StreamCopyBlocksUlt::setSourceRange(lockedPtr + srcMisalignment, copySize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList.performCpuMemcpy(cpuMemCopyInfo, nullptr, 0, nullptr));

    EXPECT_EQ(1u, NEO::StreamCopyBlocksUlt::outOfSourceRangeLoadCount);
    EXPECT_EQ(0, memcmp(hostBuffer.get(), lockedPtr + srcMisalignment, copySize));
}

HWTEST_F(CommandListCpuStreamCopyTest, givenNonUsmHostSourceWhenPerformCpuMemcpyThenNoBlockIsLoadedFromOutsideSource) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    auto *srcPtr = static_cast<uint8_t *>(hostBuffer.get()) + srcMisalignment;
    CpuMemCopyInfo cpuMemCopyInfo(devicePtr, srcPtr, copySize);
    cmdList.obtainAllocData(cpuMemCopyInfo, false);

    NEO::StreamCopyBlocksUlt::reset();
    NEO::StreamCopyBlocksUlt::setSourceRange(srcPtr, copySize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList.performCpuMemcpy(cpuMemCopyInfo, nullptr, 0, nullptr));

    EXPECT_NE(0u, NEO::StreamCopyBlocksUlt::streamLoadCount);
    EXPECT_EQ(0u, NEO::StreamCopyBlocksUlt::outOfSourceRangeLoadCount);
}

} // namespace ult
} // namespace L0
