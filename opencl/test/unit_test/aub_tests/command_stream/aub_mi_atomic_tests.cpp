/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"

#include <cstdint>

using namespace NEO;

struct MiAtomicAubFixture : public AUBFixture {
    void setUp() {
        AUBFixture::setUp(nullptr);
        auto memoryManager = this->device->getMemoryManager();

        AllocationProperties commandBufferProperties = {device->getRootDeviceIndex(),
                                                        true,
                                                        MemoryConstants::pageSize,
                                                        AllocationType::commandBuffer,
                                                        false,
                                                        device->getDeviceBitfield()};
        streamAllocation = memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);
        ASSERT_NE(nullptr, streamAllocation);

        AllocationProperties deviceBufferProperties = {device->getRootDeviceIndex(),
                                                       true,
                                                       MemoryConstants::pageSize,
                                                       AllocationType::buffer,
                                                       false,
                                                       device->getDeviceBitfield()};
        deviceSurface = memoryManager->allocateGraphicsMemoryWithProperties(deviceBufferProperties);
        ASSERT_NE(nullptr, deviceSurface);

        AllocationProperties systemBufferProperties = {device->getRootDeviceIndex(),
                                                       true,
                                                       MemoryConstants::pageSize,
                                                       AllocationType::svmCpu,
                                                       false,
                                                       device->getDeviceBitfield()};
        systemSurface = memoryManager->allocateGraphicsMemoryWithProperties(systemBufferProperties);
        ASSERT_NE(nullptr, systemSurface);

        taskStream.replaceGraphicsAllocation(streamAllocation);
        taskStream.replaceBuffer(streamAllocation->getUnderlyingBuffer(),
                                 streamAllocation->getUnderlyingBufferSize());
    }

    void tearDown() {
        auto memoryManager = this->device->getMemoryManager();
        memoryManager->freeGraphicsMemory(streamAllocation);
        memoryManager->freeGraphicsMemory(deviceSurface);
        memoryManager->freeGraphicsMemory(systemSurface);

        AUBFixture::tearDown();
    }

    void flushStream() {
        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        dispatchFlags.guardCommandBufferWithPipeControl = true;

        csr->makeResident(*deviceSurface);
        csr->makeResident(*systemSurface);
        csr->flushTask(taskStream, 0,
                       &csr->getIndirectHeap(IndirectHeapType::dynamicState, 0u),
                       &csr->getIndirectHeap(IndirectHeapType::indirectObject, 0u),
                       &csr->getIndirectHeap(IndirectHeapType::surfaceState, 0u),
                       0u, dispatchFlags, device->getDevice());

        csr->flushBatchedSubmissions();
    }

    LinearStream taskStream;
    GraphicsAllocation *streamAllocation = nullptr;
    GraphicsAllocation *deviceSurface = nullptr;
    GraphicsAllocation *systemSurface = nullptr;
};

using MiAtomicAubTest = Test<MiAtomicAubFixture>;

HWTEST_F(MiAtomicAubTest, WhenDispatchingAtomicMoveOperationThenExpectCorrectEndValues) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    auto atomicAddress = deviceSurface->getGpuAddress();

    auto expectedGpuAddressDwordOp1 = atomicAddress;
    auto expectedGpuAddressDwordOp2 = expectedGpuAddressDwordOp1 + sizeof(uint32_t);
    auto expectedGpuAddressQwordOp3 = expectedGpuAddressDwordOp2 + sizeof(uint32_t);

    uint32_t operation1dword0 = 0x10;
    EncodeAtomic<FamilyType>::programMiAtomic(taskStream, expectedGpuAddressDwordOp1,
                                              MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                              MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD,
                                              0, 0, operation1dword0, 0u);

    uint32_t operation2dword0 = 0x22;
    EncodeAtomic<FamilyType>::programMiAtomic(taskStream, expectedGpuAddressDwordOp2,
                                              MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                              MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD,
                                              0, 0, operation2dword0, 0u);

    uint32_t operation3dword0 = 0xF0;
    uint32_t operation3dword1 = 0x1F;
    uint64_t operation3qword = (static_cast<uint64_t>(operation3dword1) << 32) | operation3dword0;

    EncodeAtomic<FamilyType>::programMiAtomic(taskStream, expectedGpuAddressQwordOp3,
                                              MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_MOVE,
                                              MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD,
                                              0, 0, operation3qword, 0);

    flushStream();

    expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddressDwordOp1), &operation1dword0, sizeof(operation1dword0));
    expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddressDwordOp2), &operation2dword0, sizeof(operation2dword0));
    expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddressQwordOp3), &operation3qword, sizeof(operation3qword));
}

HWTEST_F(MiAtomicAubTest, GivenSystemMemoryWhenDispatchingAtomicMove4BytesOperationThenExpectCorrectEndValues) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    auto atomicAddress = systemSurface->getGpuAddress();

    auto expectedGpuAddressDwordOp1 = atomicAddress;
    auto expectedGpuAddressDwordOp2 = expectedGpuAddressDwordOp1 + sizeof(uint32_t);

    uint32_t operation1dword0 = 0x15;
    EncodeAtomic<FamilyType>::programMiAtomic(taskStream, expectedGpuAddressDwordOp1,
                                              MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                              MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD,
                                              0, 0, operation1dword0, 0u);

    uint32_t operation2dword0 = 0xFF;
    EncodeAtomic<FamilyType>::programMiAtomic(taskStream, expectedGpuAddressDwordOp2,
                                              MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                              MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD,
                                              0, 0, operation2dword0, 0u);

    flushStream();

    expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddressDwordOp1), &operation1dword0, sizeof(operation1dword0));
    expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddressDwordOp2), &operation2dword0, sizeof(operation2dword0));
}
