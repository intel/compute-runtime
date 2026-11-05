/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/common_matchers.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/aub_tests/command_stream/command_stream_aub_fixture.h"

namespace L0 {
namespace ult {

struct CommandStreamTestsXe3pAndLaterFixture : public CommandStreamAubFixture {
    void setUp() {
        CommandStreamAubFixture::setUp();
        this->bufferAllocation = this->allocate(MemoryConstants::pageSize, NEO::AllocationType::buffer);
        ASSERT_NE(nullptr, this->bufferAllocation);
    }

    NEO::GraphicsAllocation *bufferAllocation = nullptr;
};

using CommandStreamTestsXe3pAndLater = Test<CommandStreamTestsXe3pAndLaterFixture>;

HWTEST2_F(CommandStreamTestsXe3pAndLater, given64bDataToCompareWhenUsingIndirectSemaphoreThenOperationIsSuccessful, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    const uint64_t compareDataGpr0 = 0x1'0000'0002;
    const uint32_t compareDataGpr0Low = static_cast<uint32_t>(compareDataGpr0);
    const uint32_t compareDataGpr0High = static_cast<uint32_t>(compareDataGpr0 >> 32);

    const uint64_t compareDataMem0 = 0x2'0000'0001;
    const uint32_t compareDataMem0Low = static_cast<uint32_t>(compareDataMem0);
    const uint32_t compareDataMem0High = static_cast<uint32_t>(compareDataMem0 >> 32);

    const auto &compilerProductHelper = this->neoDevice->getRootDeviceEnvironment().getHelper<NEO::CompilerProductHelper>();
    const bool useSemaphore64bCmd = compilerProductHelper.isAvailableSemaphore64(this->neoDevice->getHardwareInfo());

    NEO::LriHelper<FamilyType>::program(this->taskStream.get(), RegisterOffsets::csGprR0, compareDataGpr0Low, true, false);
    NEO::LriHelper<FamilyType>::program(this->taskStream.get(), RegisterOffsets::csGprR0 + 4, compareDataGpr0High, true, false);

    NEO::EncodeStoreMemory<FamilyType>::programStoreDataImm(*this->taskStream, this->bufferAllocation->getGpuAddress(), compareDataMem0Low, compareDataMem0High, true, false,
                                                            nullptr);

    NEO::EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(*this->taskStream, this->bufferAllocation->getGpuAddress(), 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_SDD, false, true, true, false, useSemaphore64bCmd, nullptr);

    const uint64_t storeValue = 0x456'0000'0123;
    const uint32_t storeValueLow = static_cast<uint32_t>(storeValue);
    const uint32_t storeValueHigh = static_cast<uint32_t>(storeValue >> 32);

    NEO::EncodeStoreMemory<FamilyType>::programStoreDataImm(*this->taskStream, this->bufferAllocation->getGpuAddress() + sizeof(uint64_t), storeValueLow, storeValueHigh, true, false,
                                                            nullptr);

    this->flushStream();

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->bufferAllocation->getGpuAddress() + sizeof(uint64_t)), &storeValue, sizeof(uint64_t));
}

} // namespace ult
} // namespace L0
