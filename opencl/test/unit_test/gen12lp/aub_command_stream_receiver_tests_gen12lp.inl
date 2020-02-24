/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/aub_mem_dump/page_table_entry_bits.h"
#include "opencl/source/command_stream/aub_command_stream_receiver_hw.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/helpers/hw_helper_tests.h"
#include "opencl/test/unit_test/mocks/mock_aub_csr.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_os_context.h"
#include "test.h"

using namespace NEO;

using Gen12LPAubCommandStreamReceiverTests = Test<DeviceFixture>;

GEN12LPTEST_F(Gen12LPAubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetGUCWorkQueueItemHeaderIsCalledThenAppropriateValueDependingOnEngineTypeIsReturned) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex()));
    MockOsContext rcsOsContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    MockOsContext ccsOsContext(0, 1, aub_stream::ENGINE_CCS, PreemptionMode::Disabled, false);

    aubCsr->setupContext(ccsOsContext);
    uint32_t headerCCS = aubCsr->getGUCWorkQueueItemHeader();
    aubCsr->setupContext(rcsOsContext);
    uint32_t headerRCS = aubCsr->getGUCWorkQueueItemHeader();

    EXPECT_EQ(0x00030401u, headerCCS);
    EXPECT_EQ(0x00030001u, headerRCS);
}

GEN12LPTEST_F(Gen12LPAubCommandStreamReceiverTests, givenGraphicsAlloctionWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex()));
    MockGraphicsAllocation allocation(nullptr, 0);
    auto bits = aubCsr->getPPGTTAdditionalBits(&allocation);
    constexpr uint64_t expectedBits = BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit);

    EXPECT_EQ(expectedBits, bits);
}

GEN12LPTEST_F(Gen12LPAubCommandStreamReceiverTests, givenCCSEnabledWhenEngineMmiosAreInitializedThenExpectL3ConfigMmioIsWritten) {
    MockOsContext osContext(0, 1, aub_stream::ENGINE_CCS, PreemptionMode::Disabled, false);
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    aubCsr.setupContext(osContext);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr.stream = stream.get();

    aubCsr.initEngineMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0xB234, 0xD0000020u)));
}

GEN12LPTEST_F(Gen12LPAubCommandStreamReceiverTests, givenRCSEnabledWhenEngineMmiosAreInitializedThenExpectL3ConfigMmioIsWritten) {
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    aubCsr.setupContext(osContext);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr.stream = stream.get();

    aubCsr.initEngineMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0xB134, 0xD0000020u)));
}
