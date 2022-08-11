/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen12LPAubCommandStreamReceiverTests = Test<DeviceFixture>;

GEN12LPTEST_F(Gen12LPAubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetGUCWorkQueueItemHeaderIsCalledThenAppropriateValueDependingOnEngineTypeIsReturned) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    MockOsContext rcsOsContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    MockOsContext ccsOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular}));

    aubCsr->setupContext(ccsOsContext);
    uint32_t headerCCS = aubCsr->getGUCWorkQueueItemHeader();
    aubCsr->setupContext(rcsOsContext);
    uint32_t headerRCS = aubCsr->getGUCWorkQueueItemHeader();

    EXPECT_EQ(0x00030401u, headerCCS);
    EXPECT_EQ(0x00030001u, headerRCS);
}

GEN12LPTEST_F(Gen12LPAubCommandStreamReceiverTests, givenGraphicsAlloctionWithLocalMemoryPoolWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    MockGraphicsAllocation allocation(nullptr, 0);
    allocation.overrideMemoryPool(MemoryPool::LocalMemory);
    auto bits = aubCsr->getPPGTTAdditionalBits(&allocation);
    constexpr uint64_t expectedBits = BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) | BIT(PageTableEntry::localMemoryBit);

    EXPECT_EQ(expectedBits, bits);
}

GEN12LPTEST_F(Gen12LPAubCommandStreamReceiverTests, givenCCSEnabledWhenEngineMmiosAreInitializedThenExpectL3ConfigMmioIsWritten) {
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular}));
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    aubCsr.setupContext(osContext);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr.stream = stream.get();

    aubCsr.initEngineMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0xB234, 0xD0000020u)));
}

GEN12LPTEST_F(Gen12LPAubCommandStreamReceiverTests, givenRCSEnabledWhenEngineMmiosAreInitializedThenExpectL3ConfigMmioIsWritten) {
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    aubCsr.setupContext(osContext);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr.stream = stream.get();

    aubCsr.initEngineMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0xB134, 0xD0000020u)));
}
