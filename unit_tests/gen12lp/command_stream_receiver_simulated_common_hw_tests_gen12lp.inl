/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/helpers/hardware_context_controller.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_aub_stream.h"

#include "command_stream_receiver_simulated_hw.h"

using namespace NEO;

using Gen12LPCommandStreamReceiverSimulatedCommonHwTests = Test<DeviceFixture>;

template <typename FamilyType>
class MockSimulatedCsrHw : public CommandStreamReceiverSimulatedHw<FamilyType> {
  public:
    using CommandStreamReceiverSimulatedHw<FamilyType>::CommandStreamReceiverSimulatedHw;

    void pollForCompletion() override {}
    bool writeMemory(GraphicsAllocation &gfxAllocation) override { return true; }
    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override {}
};

GEN12LPTEST_F(Gen12LPCommandStreamReceiverSimulatedCommonHwTests, givenAubCommandStreamReceiverWhewGlobalMmiosAreInitializedThenMOCSRegistersAreConfigured) {
    MockSimulatedCsrHw<FamilyType> csrSimulatedCommonHw(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex());

    auto stream = std::make_unique<MockAubStreamMockMmioWrite>();
    csrSimulatedCommonHw.stream = stream.get();

    csrSimulatedCommonHw.initGlobalMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004000, 0x00000008)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004004, 0x00000038)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004008, 0x00000038)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000400C, 0x00000008)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004010, 0x00000018)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004014, 0x00060038)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004018, 0x00000000)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000401C, 0x00000033)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004020, 0x00060037)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004024, 0x0000003B)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004028, 0x00000032)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000402C, 0x00000036)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004030, 0x0000003A)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004034, 0x00000033)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004038, 0x00000037)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000403C, 0x0000003B)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004040, 0x00000030)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004044, 0x00000034)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004048, 0x00000038)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000404C, 0x00000031)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004050, 0x00000032)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004054, 0x00000036)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004058, 0x0000003A)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000405C, 0x00000033)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004060, 0x00000037)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004064, 0x0000003B)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004068, 0x00000032)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000406C, 0x00000036)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004070, 0x0000003A)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004074, 0x00000033)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004078, 0x00000037)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000407C, 0x0000003B)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004080, 0x00000030)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004084, 0x00000034)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004088, 0x00000038)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000408C, 0x00000031)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004090, 0x00000032)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004094, 0x00000036)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004098, 0x0000003A)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000409C, 0x00000033)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040A0, 0x00000037)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040A4, 0x0000003B)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040A8, 0x00000032)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040AC, 0x00000036)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040B0, 0x0000003A)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040B4, 0x00000033)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040B8, 0x00000037)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040BC, 0x0000003B)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040C0, 0x00000038)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040C4, 0x00000034)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040C8, 0x00000038)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040CC, 0x00000031)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040D0, 0x00000032)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040D4, 0x00000036)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040D8, 0x0000003A)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040DC, 0x00000033)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040E0, 0x00000037)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040E4, 0x0000003B)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040E8, 0x00000032)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040EC, 0x00000036)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040F0, 0x00000038)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040F4, 0x00000038)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040F8, 0x00000038)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x000040FC, 0x00000038)));
}

GEN12LPTEST_F(Gen12LPCommandStreamReceiverSimulatedCommonHwTests, givenAubCommandStreamReceiverWhenGlobalMmiosAreInitializedThenLNCFRegistersAreConfigured) {
    MockSimulatedCsrHw<FamilyType> csrSimulatedCommonHw(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex());

    auto stream = std::make_unique<MockAubStreamMockMmioWrite>();
    csrSimulatedCommonHw.stream = stream.get();

    csrSimulatedCommonHw.initGlobalMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B020, 0x00300010)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B024, 0x00300010)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B028, 0x00300030)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B02C, 0x00000000)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B030, 0x0030001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B034, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B038, 0x0000001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B03C, 0x00000000)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B040, 0x00100000)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B044, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B048, 0x0010001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B04C, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B050, 0x0030001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B054, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B058, 0x0000001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B05C, 0x00000000)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B060, 0x00100000)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B064, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B068, 0x0010001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B06C, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B070, 0x0030001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B074, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B078, 0x0000001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B07C, 0x00000000)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B080, 0x00000030)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B084, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B088, 0x0010001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B08C, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B090, 0x0030001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B094, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B098, 0x00300010)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B09C, 0x00300010)));
}
