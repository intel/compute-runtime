/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

namespace NEO {
class Device;
} // namespace NEO

using namespace NEO;

using PvcAndLaterBufferTests = ::testing::Test;

HWTEST2_F(PvcAndLaterBufferTests, WhenAllocatingBufferThenGpuAddressIsFromHeapExtended, IsAtLeastXeHpcCore) {
    if (is32bit || defaultHwInfo->capabilityTable.gpuAddressSpace != maxNBitValue(57)) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(true);

    initPlatform();

    MockContext context(platform()->getClDevice(0));

    size_t size = 0x1000;
    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(
        Buffer::create(
            &context,
            CL_MEM_READ_WRITE,
            size,
            nullptr,
            retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto graphicsAllocation = buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, graphicsAllocation);

    auto gmmHelper = context.getDevice(0)->getGmmHelper();
    auto gpuAddress = gmmHelper->decanonize(graphicsAllocation->getGpuAddress());
    auto extendedHeapBase = context.memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExtended);
    auto extendedHeapLimit = context.memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExtended);

    EXPECT_EQ(extendedHeapBase, maxNBitValue(57 - 1) + 1);
    EXPECT_EQ(extendedHeapLimit, extendedHeapBase + maxNBitValue(48));

    EXPECT_GT(gpuAddress, extendedHeapBase);
    EXPECT_LT(gpuAddress, extendedHeapLimit);
}

HWTEST2_F(PvcAndLaterBufferTests, WhenAllocatingRtBufferThenGpuAddressFromHeapStandard64Kb, IsAtLeastXeHpcCore) {
    if (is32bit || defaultHwInfo->capabilityTable.gpuAddressSpace != maxNBitValue(57)) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(true);

    initPlatform();

    MockContext context(platform()->getClDevice(0));

    auto retVal = CL_SUCCESS;
    std::unique_ptr<Buffer> rtBuffer;
    rtBuffer.reset(Buffer::create(&context,
                                  ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, CL_MEM_48BIT_RESOURCE_INTEL, 0,
                                                                                   &context.getDevice(0)->getDevice()),
                                  CL_MEM_READ_WRITE,
                                  CL_MEM_48BIT_RESOURCE_INTEL,
                                  MemoryConstants::pageSize, nullptr, retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, rtBuffer);

    EXPECT_TRUE(isValueSet(rtBuffer->getFlagsIntel(), CL_MEM_48BIT_RESOURCE_INTEL));

    auto graphicsAllocation = rtBuffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, graphicsAllocation);

    auto gmmHelper = context.getDevice(0)->getGmmHelper();
    auto gpuAddress = gmmHelper->decanonize(graphicsAllocation->getGpuAddress());
    auto standard64KbHeapBase = context.memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard64KB);
    auto standard64KbHeapLimit = context.memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard64KB);

    EXPECT_GT(gpuAddress, standard64KbHeapBase);
    EXPECT_LT(gpuAddress, standard64KbHeapLimit);
}

HWTEST2_F(PvcAndLaterBufferTests, givenCompressedBufferInSystemAndBlitterSupportedWhenCreatingBufferThenDoNotUseBlitterLogicForLocalMem, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore;
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    VariableBackup<BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup{
        &BlitHelperFunctions::blitMemoryToAllocation};
    debugManager.flags.RenderCompressedBuffersEnabled.set(true);
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;

    UltClDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    auto pMockContext = std::make_unique<MockContext>(pDevice);

    static_cast<MockMemoryManager *>(pDevice->getExecutionEnvironment()->memoryManager.get())->enable64kbpages[0] = true;
    static_cast<MockMemoryManager *>(pDevice->getExecutionEnvironment()->memoryManager.get())->localMemorySupported[0] = false;

    blitMemoryToAllocationFuncBackup = [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset,
                                          const void *hostPtr, Vec3<size_t> size) -> NEO::BlitOperationResult {
        ADD_FAILURE();
        return BlitOperationResult::fail;
    };
    cl_mem_flags flags = CL_MEM_COPY_HOST_PTR | CL_MEM_COMPRESSED_HINT_INTEL;
    uint32_t hostPtr = 0;
    cl_int retVal = CL_SUCCESS;
    auto bufferForBlt = clUniquePtr(Buffer::create(pMockContext.get(), flags, 2000, &hostPtr, retVal));
}
