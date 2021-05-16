/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/fixtures/device_host_queue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;
using namespace DeviceHostQueue;

GEN12LPTEST_F(DeviceQueueHwTest, givenDeviceQueueWhenRunningOnCCsThenFfidSkipOffsetIsAddedToBlockKernelStartPointer) {
    auto device = pContext->getDevice(0);
    std::unique_ptr<MockParentKernel> mockParentKernel(MockParentKernel::create(*pContext));
    KernelInfo *blockInfo = const_cast<KernelInfo *>(mockParentKernel->mockProgram->blockKernelManager->getBlockKernelInfo(0));
    blockInfo->createKernelAllocation(device->getDevice(), false);
    ASSERT_NE(nullptr, blockInfo->getGraphicsAllocation());
    blockInfo->kernelDescriptor.entryPoints.skipSetFFIDGP = 0x1234;

    auto &hwInfo = const_cast<HardwareInfo &>(device->getHardwareInfo());
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    hwInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

    uint64_t expectedOffset = blockInfo->getGraphicsAllocation()->getGpuAddressToPatch() + blockInfo->kernelDescriptor.entryPoints.skipSetFFIDGP;
    uint64_t offset = MockDeviceQueueHw<FamilyType>::getBlockKernelStartPointer(device->getDevice(), blockInfo, true);
    EXPECT_EQ(expectedOffset, offset);

    expectedOffset = blockInfo->getGraphicsAllocation()->getGpuAddressToPatch();
    offset = MockDeviceQueueHw<FamilyType>::getBlockKernelStartPointer(device->getDevice(), blockInfo, false);
    EXPECT_EQ(expectedOffset, offset);

    hwInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_A1, hwInfo);

    expectedOffset = blockInfo->getGraphicsAllocation()->getGpuAddressToPatch();
    offset = MockDeviceQueueHw<FamilyType>::getBlockKernelStartPointer(device->getDevice(), blockInfo, true);
    EXPECT_EQ(expectedOffset, offset);

    offset = MockDeviceQueueHw<FamilyType>::getBlockKernelStartPointer(device->getDevice(), blockInfo, false);
    EXPECT_EQ(expectedOffset, offset);
}
