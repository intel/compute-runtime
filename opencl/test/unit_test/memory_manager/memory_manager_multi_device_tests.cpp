/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/fixtures/memory_allocator_multi_device_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_manager_fixture.h"
#include "opencl/test/unit_test/helpers/execution_environment_helper.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_os_context.h"
#include "test.h"

using namespace NEO;

using MemoryManagerMultiDeviceTest = MemoryAllocatorMultiDeviceFixture<10>;

TEST_P(MemoryManagerMultiDeviceTest, givenRootDeviceIndexSpecifiedWhenAllocateGraphicsMemoryIsCalledThenGraphicsAllocationHasTheSameRootDeviceIndex) {
    std::vector<GraphicsAllocation::AllocationType> allocationTypes{GraphicsAllocation::AllocationType::BUFFER,
                                                                    GraphicsAllocation::AllocationType::KERNEL_ISA};
    for (auto allocationType : allocationTypes) {
        for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < getNumRootDevices(); ++rootDeviceIndex) {
            AllocationProperties properties{rootDeviceIndex, true, MemoryConstants::pageSize, allocationType, false, false, 0};

            auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);

            gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, (void *)0x1234);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);

            gfxAllocation = memoryManager->allocateGraphicsMemoryInPreferredPool(properties, nullptr);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);

            gfxAllocation = memoryManager->allocateGraphicsMemoryInPreferredPool(properties, (void *)0x1234);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);

            if (isOsAgnosticMemoryManager) {
                gfxAllocation = memoryManager->createGraphicsAllocationFromSharedHandle((osHandle)1u, properties, false);
                ASSERT_NE(gfxAllocation, nullptr);
                EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
                memoryManager->freeGraphicsMemory(gfxAllocation);

                gfxAllocation = memoryManager->createGraphicsAllocationFromSharedHandle((osHandle)1u, properties, true);
                ASSERT_NE(gfxAllocation, nullptr);
                EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
                memoryManager->freeGraphicsMemory(gfxAllocation);
            }
        }
    }
}

INSTANTIATE_TEST_CASE_P(MemoryManagerType, MemoryManagerMultiDeviceTest, ::testing::Bool());
