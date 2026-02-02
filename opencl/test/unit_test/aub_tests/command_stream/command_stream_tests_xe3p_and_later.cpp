/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/test_macros/header/common_matchers.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"

using namespace NEO;

struct CommandStreamTestsXe3pAndLater : public AUBFixture, public ::testing::Test {
    void SetUp() override {
        AUBFixture::setUp(defaultHwInfo.get());

        streamAllocation = this->device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::commandBuffer, device->getDeviceBitfield()});
        bufferAllocation = this->device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, device->getDeviceBitfield()});
        taskStream = std::make_unique<LinearStream>(streamAllocation);

        csr->makeResident(*bufferAllocation);
    }
    void TearDown() override {
        this->device->getMemoryManager()->freeGraphicsMemory(streamAllocation);
        this->device->getMemoryManager()->freeGraphicsMemory(bufferAllocation);
        AUBFixture::tearDown();
    }

    void flushStream() {
        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        dispatchFlags.guardCommandBufferWithPipeControl = true;

        csr->flushTask(*taskStream, 0,
                       &csr->getIndirectHeap(IndirectHeapType::dynamicState, 0u),
                       &csr->getIndirectHeap(IndirectHeapType::indirectObject, 0u),
                       &csr->getIndirectHeap(IndirectHeapType::surfaceState, 0u),
                       0u, dispatchFlags, device->getDevice());

        csr->flushBatchedSubmissions();
    }

    static constexpr size_t bufferSize = MemoryConstants::pageSize;
    std::unique_ptr<LinearStream> taskStream;
    GraphicsAllocation *streamAllocation = nullptr;
    GraphicsAllocation *bufferAllocation = nullptr;
};

HWTEST2_F(CommandStreamTestsXe3pAndLater, given64bDataToCompareWhenUsingIndirectSemaphoreThenOperationIsSuccessful, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    const uint64_t compareDataGpr0 = 0x1'0000'0002;
    const uint32_t compareDataGpr0Low = static_cast<uint32_t>(compareDataGpr0);
    const uint32_t compareDataGpr0High = static_cast<uint32_t>(compareDataGpr0 >> 32);

    const uint64_t compareDataMem0 = 0x2'0000'0001;
    const uint32_t compareDataMem0Low = static_cast<uint32_t>(compareDataMem0);
    const uint32_t compareDataMem0High = static_cast<uint32_t>(compareDataMem0 >> 32);

    auto &productHelper = this->device->getProductHelper();
    auto *releaseHelper = this->device->getDevice().getReleaseHelper();
    const bool useSemaphore64bCmd = productHelper.isAvailableSemaphore64(releaseHelper);

    LriHelper<FamilyType>::program(taskStream.get(), RegisterOffsets::csGprR0, compareDataGpr0Low, true, false);
    LriHelper<FamilyType>::program(taskStream.get(), RegisterOffsets::csGprR0 + 4, compareDataGpr0High, true, false);

    EncodeStoreMemory<FamilyType>::programStoreDataImm(*taskStream, bufferAllocation->getGpuAddress(), compareDataMem0Low, compareDataMem0High, true, false,
                                                       nullptr);

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(*taskStream, bufferAllocation->getGpuAddress(), 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_SDD, false, true, true, false, useSemaphore64bCmd, nullptr);

    const uint64_t storeValue = 0x456'0000'0123;
    const uint32_t storeValueLow = static_cast<uint32_t>(storeValue);
    const uint32_t storeValueHigh = static_cast<uint32_t>(storeValue >> 32);

    EncodeStoreMemory<FamilyType>::programStoreDataImm(*taskStream, bufferAllocation->getGpuAddress() + sizeof(uint64_t), storeValueLow, storeValueHigh, true, false,
                                                       nullptr);

    flushStream();

    expectMemory<FamilyType>(reinterpret_cast<void *>(bufferAllocation->getGpuAddress() + sizeof(uint64_t)), &storeValue, sizeof(uint64_t));
}
