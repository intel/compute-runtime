/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include <atomic>
#include <thread>

namespace L0 {
namespace ult {

using CommandListVirtualReservationMtTest = Test<DeviceFixture>;

// addVirtualReservationToResidency() walks every mapping of the reservation backing the appended
// pointer, including the ones the application is still free to unmap. Race a resolve of one
// sub-range against map/unmap of a sibling sub-range: without the reservation map lock held for the
// traversal, the walk observes mappedAllocations while zeVirtualMemUnmap erases from it and deletes
// the MemoryMappedRange it is reading.
HWTEST_F(CommandListVirtualReservationMtTest, givenSiblingMappingRemappedWhileResolvingReservedAllocationThenReservationMapTraversalIsSerialized) {
    driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    const size_t size = MemoryConstants::pageSize64k;
    const size_t reservationSize = size * 2;

    void *reservedBase = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->reserveVirtualMem(nullptr, reservationSize, &reservedBase));
    ASSERT_NE(nullptr, reservedBase);

    ze_physical_mem_desc_t desc = {};
    desc.size = size;
    ze_physical_mem_handle_t phPhysicalMemoryResident = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->createPhysicalMem(device->toHandle(), &desc, &phPhysicalMemoryResident));
    ze_physical_mem_handle_t phPhysicalMemoryChurned = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->createPhysicalMem(device->toHandle(), &desc, &phPhysicalMemoryChurned));

    // The first sub-range stays mapped for the whole test and is the one being appended.
    ASSERT_EQ(ZE_RESULT_SUCCESS, context->mapVirtualMem(reservedBase, size, phPhysicalMemoryResident, 0, ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE));
    void *churnedAddress = reinterpret_cast<void *>(reinterpret_cast<uint64_t>(reservedBase) + size);

    constexpr uint32_t iterationCount = 200;
    std::atomic_bool started = false;
    std::atomic_bool resolverDone = false;

    auto resolverBody = [&]() {
        while (!started.load()) {
            std::this_thread::yield();
        }
        for (uint32_t i = 0; i < iterationCount; i++) {
            auto outData = commandList->resolveAlignedAllocation(device, reservedBase, size, nullptr, {});
            EXPECT_NE(nullptr, outData.alloc);
        }
        resolverDone = true;
    };

    // Map and unmap the sibling sub-range underneath the resolver. The application never references
    // this range through the command list, so this sequence is legal while an append is in flight.
    auto remapperBody = [&]() {
        while (!started.load()) {
            std::this_thread::yield();
        }
        while (!resolverDone.load()) {
            if (ZE_RESULT_SUCCESS != context->mapVirtualMem(churnedAddress, size, phPhysicalMemoryChurned, 0, ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE)) {
                continue;
            }
            context->unMapVirtualMem(churnedAddress, size);
        }
    };

    std::thread resolver(resolverBody);
    std::thread remapper(remapperBody);

    started = true;

    resolver.join();
    remapper.join();

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->unMapVirtualMem(reservedBase, size));
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeVirtualMem(reservedBase, reservationSize));
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->destroyPhysicalMem(phPhysicalMemoryResident));
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->destroyPhysicalMem(phPhysicalMemoryChurned));
}

} // namespace ult
} // namespace L0
