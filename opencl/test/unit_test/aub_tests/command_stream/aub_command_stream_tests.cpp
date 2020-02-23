/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/os_context.h"
#include "opencl/source/command_stream/aub_command_stream_receiver_hw.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"
#include "test.h"

#include "aub_command_stream_fixture.h"

#include <cstdint>

using namespace NEO;

struct AUBFixture : public AUBCommandStreamFixture,
                    public CommandQueueFixture,
                    public DeviceFixture {

    using AUBCommandStreamFixture::SetUp;
    using CommandQueueFixture::SetUp;

    void SetUp() {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(nullptr, pClDevice, 0);
        AUBCommandStreamFixture::SetUp(pCmdQ);
    }

    void TearDown() override {
        AUBCommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    template <typename FamilyType>
    void testNoopIdXcs(aub_stream::EngineType engineType) {
        pCommandStreamReceiver->getOsContext().getEngineType() = engineType;

        typedef typename FamilyType::MI_NOOP MI_NOOP;

        auto pCmd = (MI_NOOP *)pCS->getSpace(sizeof(MI_NOOP) * 4);

        uint32_t noopId = 0xbaadd;
        auto noop = FamilyType::cmdInitNoop;
        *pCmd++ = noop;
        *pCmd++ = noop;
        *pCmd++ = noop;
        noop.TheStructure.Common.IdentificationNumberRegisterWriteEnable = true;
        noop.TheStructure.Common.IdentificationNumber = noopId;
        *pCmd++ = noop;

        CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(*pCS, nullptr);
        CommandStreamReceiverHw<FamilyType>::alignToCacheLine(*pCS);
        BatchBuffer batchBuffer{pCS->getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, pCS->getUsed(), pCS, nullptr};
        ResidencyContainer allocationsForResidency;
        pCommandStreamReceiver->flush(batchBuffer, allocationsForResidency);

        AUBCommandStreamFixture::getSimulatedCsr<FamilyType>()->pollForCompletionImpl();
        auto mmioBase = CommandStreamReceiverSimulatedCommonHw<FamilyType>::getCsTraits(engineType).mmioBase;
        AUBCommandStreamFixture::expectMMIO<FamilyType>(AubMemDump::computeRegisterOffset(mmioBase, 0x2094), noopId);
    }
};

typedef Test<AUBFixture> AUBcommandstreamTests;

HWTEST_F(AUBcommandstreamTests, testFlushTwice) {
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(*pCS, nullptr);
    CommandStreamReceiverHw<FamilyType>::alignToCacheLine(*pCS);
    BatchBuffer batchBuffer{pCS->getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, pCS->getUsed(), pCS, nullptr};
    ResidencyContainer allocationsForResidency;

    pCommandStreamReceiver->flush(batchBuffer, allocationsForResidency);
    BatchBuffer batchBuffer2{pCS->getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, pCS->getUsed(), pCS, nullptr};
    ResidencyContainer allocationsForResidency2;
    pCommandStreamReceiver->flush(batchBuffer2, allocationsForResidency);
    AUBCommandStreamFixture::getSimulatedCsr<FamilyType>()->pollForCompletion();
}

HWTEST_F(AUBcommandstreamTests, testNoopIdRcs) {
    testNoopIdXcs<FamilyType>(aub_stream::ENGINE_RCS);
}

HWTEST_F(AUBcommandstreamTests, testNoopIdBcs) {
    testNoopIdXcs<FamilyType>(aub_stream::ENGINE_BCS);
}

HWTEST_F(AUBcommandstreamTests, testNoopIdVcs) {
    testNoopIdXcs<FamilyType>(aub_stream::ENGINE_VCS);
}

HWTEST_F(AUBcommandstreamTests, testNoopIdVecs) {
    testNoopIdXcs<FamilyType>(aub_stream::ENGINE_VECS);
}

TEST_F(AUBcommandstreamTests, makeResident) {
    uint8_t buffer[0x10000];
    size_t size = sizeof(buffer);
    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    auto graphicsAllocation = createResidentAllocationAndStoreItInCsr(buffer, size);
    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    commandStreamReceiver.processResidency(allocationsForResidency, 0u);
}

HWTEST_F(AUBcommandstreamTests, expectMemorySingle) {
    uint32_t buffer = 0xdeadbeef;
    size_t size = sizeof(buffer);
    auto graphicsAllocation = createResidentAllocationAndStoreItInCsr(&buffer, size);
    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    pCommandStreamReceiver->processResidency(allocationsForResidency, 0u);

    AUBCommandStreamFixture::expectMemory<FamilyType>(&buffer, &buffer, size);
}

HWTEST_F(AUBcommandstreamTests, expectMemoryLarge) {
    size_t sizeBuffer = 0x100001;
    auto buffer = new uint8_t[sizeBuffer];

    for (size_t index = 0; index < sizeBuffer; ++index) {
        buffer[index] = static_cast<uint8_t>(index);
    }

    auto graphicsAllocation = createResidentAllocationAndStoreItInCsr(buffer, sizeBuffer);
    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    pCommandStreamReceiver->processResidency(allocationsForResidency, 0u);

    AUBCommandStreamFixture::expectMemory<FamilyType>(buffer, buffer, sizeBuffer);
    delete[] buffer;
}
