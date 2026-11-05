/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/aub_tests/command_stream/command_stream_aub_fixture.h"

#include <cstdint>
#include <memory>

namespace L0 {
namespace ult {

struct AUBCommandStreamL0Fixture : public CommandStreamAubFixture {
    NEO::GraphicsAllocation *createResidentAllocationAndStoreItInCsr(const void *address, size_t size) {
        auto allocation = this->csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(NEO::MockAllocationProperties{this->csr->getRootDeviceIndex(), false, size}, address);
        this->csr->makeResidentHostPtrAllocation(allocation);
        this->csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<NEO::GraphicsAllocation>(allocation), NEO::TEMPORARY_ALLOCATION);
        return allocation;
    }
};

using AUBcommandstreamTests = Test<AUBCommandStreamL0Fixture>;

HWTEST_F(AUBcommandstreamTests, WhenFlushingTwiceThenCompletes) {
    NEO::CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(*this->taskStream, nullptr);
    NEO::EncodeNoop<FamilyType>::alignToCacheLine(*this->taskStream);
    NEO::BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(this->taskStream->getGraphicsAllocation(), this->taskStream.get(), this->taskStream->getUsed());
    NEO::ResidencyContainer allocationsForResidency;
    this->csr->setLatestSentTaskCount(this->csr->peekTaskCount() + 1);
    this->csr->flush(batchBuffer, allocationsForResidency);

    NEO::BatchBuffer batchBuffer2 = BatchBufferHelper::createDefaultBatchBuffer(this->taskStream->getGraphicsAllocation(), this->taskStream.get(), this->taskStream->getUsed());
    this->csr->flush(batchBuffer2, allocationsForResidency);
    this->getSimulatedCsr<FamilyType>()->pollForCompletion();
}

HWTEST_F(AUBcommandstreamTests, WhenCreatingResidentAllocationThenAllocationIsResident) {
    uint8_t buffer[0x10000];
    size_t size = sizeof(buffer);

    this->getSimulatedCsr<FamilyType>()->initializeEngine();

    auto graphicsAllocation = this->createResidentAllocationAndStoreItInCsr(buffer, size);
    NEO::ResidencyContainer allocationsForResidency = {graphicsAllocation};
    this->csr->processResidency(allocationsForResidency, 0u);
}

HWTEST_F(AUBcommandstreamTests, GivenSingleAllocationWhenCreatingResidentAllocationThenAubIsCorrect) {
    uint32_t buffer = 0xdeadbeef;
    size_t size = sizeof(buffer);

    this->getSimulatedCsr<FamilyType>()->initializeEngine();

    auto graphicsAllocation = this->createResidentAllocationAndStoreItInCsr(&buffer, size);
    NEO::ResidencyContainer allocationsForResidency = {graphicsAllocation};
    this->csr->processResidency(allocationsForResidency, 0u);

    this->expectMemory<FamilyType>(&buffer, &buffer, size);
}

HWTEST_F(AUBcommandstreamTests, GivenMultipleAllocationsWhenCreatingResidentAllocationThenAubIsCorrect) {
    size_t sizeBuffer = 0x100001;
    auto buffer = std::make_unique<uint8_t[]>(sizeBuffer);

    for (size_t index = 0; index < sizeBuffer; ++index) {
        buffer[index] = static_cast<uint8_t>(index);
    }

    this->getSimulatedCsr<FamilyType>()->initializeEngine();

    auto graphicsAllocation = this->createResidentAllocationAndStoreItInCsr(buffer.get(), sizeBuffer);
    NEO::ResidencyContainer allocationsForResidency = {graphicsAllocation};
    this->csr->processResidency(allocationsForResidency, 0u);

    this->expectMemory<FamilyType>(buffer.get(), buffer.get(), sizeBuffer);
}

} // namespace ult
} // namespace L0
