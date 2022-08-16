/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "aub_command_stream_fixture.h"

#include <cstdint>

using namespace NEO;

struct AUBFixture : public AUBCommandStreamFixture,
                    public CommandQueueFixture,
                    public ClDeviceFixture {

    using AUBCommandStreamFixture::setUp;
    using CommandQueueFixture::setUp;

    void setUp() {
        ClDeviceFixture::setUp();
        CommandQueueFixture::setUp(nullptr, pClDevice, 0);
        AUBCommandStreamFixture::setUp(pCmdQ);
    }

    void tearDown() {
        AUBCommandStreamFixture::tearDown();
        CommandQueueFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    template <typename FamilyType>
    void testNoopIdXcs(aub_stream::EngineType engineType) {
        static_cast<MockOsContext &>(pCommandStreamReceiver->getOsContext()).engineType = engineType;

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
        EncodeNoop<FamilyType>::alignToCacheLine(*pCS);
        BatchBuffer batchBuffer{pCS->getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, pCS->getUsed(), pCS, nullptr, false};
        ResidencyContainer allocationsForResidency;
        pCommandStreamReceiver->flush(batchBuffer, allocationsForResidency);

        AUBCommandStreamFixture::getSimulatedCsr<FamilyType>()->pollForCompletionImpl();
        auto mmioBase = CommandStreamReceiverSimulatedCommonHw<FamilyType>::getCsTraits(engineType).mmioBase;
        AUBCommandStreamFixture::expectMMIO<FamilyType>(AubMemDump::computeRegisterOffset(mmioBase, 0x2094), noopId);
    }
};

using AUBcommandstreamTests = Test<AUBFixture>;

HWTEST_F(AUBcommandstreamTests, WhenFlushingTwiceThenCompletes) {
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(*pCS, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(*pCS);
    BatchBuffer batchBuffer{pCS->getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, pCS->getUsed(), pCS, nullptr, false};
    ResidencyContainer allocationsForResidency;

    pCommandStreamReceiver->flush(batchBuffer, allocationsForResidency);
    BatchBuffer batchBuffer2{pCS->getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, pCS->getUsed(), pCS, nullptr, false};
    pCommandStreamReceiver->flush(batchBuffer2, allocationsForResidency);
    AUBCommandStreamFixture::getSimulatedCsr<FamilyType>()->pollForCompletion();
}

HWTEST_F(AUBcommandstreamTests, GivenRcsWhenTestingNoopIdThenAubIsCorrect) {
    testNoopIdXcs<FamilyType>(aub_stream::ENGINE_RCS);
}

HWTEST_F(AUBcommandstreamTests, GivenBcsWhenTestingNoopIdThenAubIsCorrect) {
    testNoopIdXcs<FamilyType>(aub_stream::ENGINE_BCS);
}

HWTEST_F(AUBcommandstreamTests, GivenVcsWhenTestingNoopIdThenAubIsCorrect) {
    testNoopIdXcs<FamilyType>(aub_stream::ENGINE_VCS);
}

HWTEST_F(AUBcommandstreamTests, GivenVecsWhenTestingNoopIdThenAubIsCorrect) {
    testNoopIdXcs<FamilyType>(aub_stream::ENGINE_VECS);
}

HWTEST_F(AUBcommandstreamTests, WhenCreatingResidentAllocationThenAllocationIsResident) {
    uint8_t buffer[0x10000];
    size_t size = sizeof(buffer);

    getSimulatedCsr<FamilyType>()->initializeEngine();

    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
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
