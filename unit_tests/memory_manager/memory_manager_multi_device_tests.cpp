/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/memory_constants.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/fixtures/memory_allocator_multi_device_fixture.h"
#include "unit_tests/fixtures/memory_manager_fixture.h"
#include "unit_tests/helpers/execution_environment_helper.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_os_context.h"

using namespace NEO;

using MemoryManagerMultiDeviceTest = Test<MemoryAllocatorMultiDeviceFixture<10>>;

TEST_F(MemoryManagerMultiDeviceTest, givenRootDeviceIndexSpecifiedWhenAllocateGraphicsMemoryIsCalledThenGraphicsAllocationHasTheSameRootDeviceIndex) {
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
