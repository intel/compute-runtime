/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryManagerTest, givenEnabledLocalMemoryWhenAllocatingSharedResourceCopyThenLocalMemoryAllocationIsReturnedAndGpuAddresIsInStandard64kHeap) {
    UltDeviceFactory deviceFactory{1, 0};
    HardwareInfo localPlatformDevice = {};

    localPlatformDevice = *defaultHwInfo;
    localPlatformDevice.featureTable.flags.ftrLocalMemory = true;

    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(&localPlatformDevice, 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    MockMemoryManager memoryManager(false, true, *executionEnvironment);

    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, deviceFactory.rootDevices[0]);
    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(mockRootDeviceIndex, imgInfo, true, memoryProperties, localPlatformDevice, mockDeviceBitfield, true);
    allocProperties.allocationType = AllocationType::sharedResourceCopy;

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);

    auto gmmHelper = memoryManager.getGmmHelper(allocation->getRootDeviceIndex());
    EXPECT_LT(gmmHelper->canonize(memoryManager.getGfxPartition(allocation->getRootDeviceIndex())->getHeapBase(HeapIndex::heapStandard64KB)), allocation->getGpuAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager.getGfxPartition(allocation->getRootDeviceIndex())->getHeapLimit(HeapIndex::heapStandard64KB)), allocation->getGpuAddress());
    EXPECT_EQ(0llu, allocation->getGpuBaseAddress());

    memoryManager.freeGraphicsMemory(allocation);
}
