/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "aub_command_stream_fixture.h"

#include <cstdint>

using namespace NEO;

using AUBcommandstreamTests = Test<AUBCommandStreamFixture>;

HWTEST_F(AUBcommandstreamTests, WhenFlushingTwiceThenCompletes) {
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(*pCS, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(*pCS);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(pCS->getGraphicsAllocation(), pCS, pCS->getUsed());
    ResidencyContainer allocationsForResidency;

    pCommandStreamReceiver->setLatestSentTaskCount(pCommandStreamReceiver->peekTaskCount() + 1);

    pCommandStreamReceiver->flush(batchBuffer, allocationsForResidency);
    BatchBuffer batchBuffer2 = BatchBufferHelper::createDefaultBatchBuffer(pCS->getGraphicsAllocation(), pCS, pCS->getUsed());
    pCommandStreamReceiver->flush(batchBuffer2, allocationsForResidency);
    AUBCommandStreamFixture::getSimulatedCsr<FamilyType>()->pollForCompletion();
}

HWTEST_F(AUBcommandstreamTests, WhenCreatingResidentAllocationThenAllocationIsResident) {
    uint8_t buffer[0x10000];
    size_t size = sizeof(buffer);

    getSimulatedCsr<FamilyType>()->initializeEngine();

    auto &commandStreamReceiver = device->getGpgpuCommandStreamReceiver();
    auto graphicsAllocation = createResidentAllocationAndStoreItInCsr(buffer, size);
    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    commandStreamReceiver.processResidency(allocationsForResidency, 0u);
}

HWTEST_F(AUBcommandstreamTests, GivenSingleAllocationWhenCreatingResidentAllocationThenAubIsCorrect) {
    uint32_t buffer = 0xdeadbeef;
    size_t size = sizeof(buffer);

    getSimulatedCsr<FamilyType>()->initializeEngine();

    auto graphicsAllocation = createResidentAllocationAndStoreItInCsr(&buffer, size);
    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    pCommandStreamReceiver->processResidency(allocationsForResidency, 0u);

    AUBCommandStreamFixture::expectMemory<FamilyType>(&buffer, &buffer, size);
}

HWTEST_F(AUBcommandstreamTests, GivenMultipleAllocationsWhenCreatingResidentAllocationThenAubIsCorrect) {
    size_t sizeBuffer = 0x100001;
    auto buffer = new uint8_t[sizeBuffer];

    for (size_t index = 0; index < sizeBuffer; ++index) {
        buffer[index] = static_cast<uint8_t>(index);
    }

    getSimulatedCsr<FamilyType>()->initializeEngine();

    auto graphicsAllocation = createResidentAllocationAndStoreItInCsr(buffer, sizeBuffer);
    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    pCommandStreamReceiver->processResidency(allocationsForResidency, 0u);

    AUBCommandStreamFixture::expectMemory<FamilyType>(buffer, buffer, sizeBuffer);
    delete[] buffer;
}
