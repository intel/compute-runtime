/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_aub_stream.h"
#include "shared/test/common/mocks/mock_csr_simulated_common_hw.h"
#include "shared/test/common/test_macros/test.h"

using XeHPAndLaterMockSimulatedCsrHwTests = Test<DeviceFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterMockSimulatedCsrHwTests, givenLocalMemoryEnabledWhenGlobalMmiosAreInitializedThenLmemIsInitializedAndLmemCfgMmioIsWritten) {
    std::unique_ptr<MockSimulatedCsrHw<FamilyType>> csrSimulatedCommonHw(new MockSimulatedCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    csrSimulatedCommonHw->localMemoryEnabled = true;

    auto stream = std::make_unique<MockAubStreamMockMmioWrite>();
    csrSimulatedCommonHw->stream = stream.get();
    csrSimulatedCommonHw->initGlobalMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00101010, 0x00000080u)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000cf58, 0x80000000u)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterMockSimulatedCsrHwTests, givenAUBDumpForceAllToLocalMemoryWhenGlobalMmiosAreInitializedThenLmemIsInitializedAndLmemCfgMmioIsWritten) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    std::unique_ptr<MockSimulatedCsrHw<FamilyType>> csrSimulatedCommonHw(new MockSimulatedCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    auto stream = std::make_unique<MockAubStreamMockMmioWrite>();
    csrSimulatedCommonHw->stream = stream.get();
    csrSimulatedCommonHw->initGlobalMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00101010, 0x00000080u)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000cf58, 0x80000000u)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterMockSimulatedCsrHwTests, givenAubCommandStreamReceiverWhenGlobalMmiosAreInitializedThenMOCSRegistersAreConfigured) {
    MockSimulatedCsrHw<FamilyType> csrSimulatedCommonHw(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

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

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterMockSimulatedCsrHwTests, givenAubCommandStreamReceiverWhenGlobalMmiosAreInitializedThenLNCFRegistersAreConfigured) {
    MockSimulatedCsrHw<FamilyType> csrSimulatedCommonHw(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

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
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B080, 0x00300030)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B084, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B088, 0x0010001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B08C, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B090, 0x0030001F)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B094, 0x00170013)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B098, 0x00300010)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B09C, 0x00300010)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterMockSimulatedCsrHwTests, givenAubCommandStreamReceiverWhenGlobalMmiosAreInitializedThenPerfMmioRegistersAreConfigured) {
    MockSimulatedCsrHw<FamilyType> csrSimulatedCommonHw(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    auto stream = std::make_unique<MockAubStreamMockMmioWrite>();
    csrSimulatedCommonHw.stream = stream.get();

    csrSimulatedCommonHw.initGlobalMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B004, 0x2FC0100B)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000B404, 0x00000160)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00008708, 0x00000000)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterMockSimulatedCsrHwTests, givenAubCommandStreamReceiverWhenGlobalMmiosAreInitializedThenTRTTRegistersAreConfigured) {
    MockSimulatedCsrHw<FamilyType> csrSimulatedCommonHw(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    auto stream = std::make_unique<MockAubStreamMockMmioWrite>();
    csrSimulatedCommonHw.stream = stream.get();

    csrSimulatedCommonHw.initGlobalMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004410, 0xffffffff)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004414, 0xfffffffe)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004404, 0x000000ff)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004408, 0x00000000)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000440C, 0x00000000)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004400, 0x00000001)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x00004DFC, 0x00000000)));
}

class XeHPAndLaterTileRangeRegisterTest : public DeviceFixture, public ::testing::Test {
  public:
    template <typename FamilyType>
    void setUpImpl() {
        hardwareInfo = *defaultHwInfo;
        hardwareInfoSetup[hardwareInfo.platform.eProductFamily](&hardwareInfo, true, 0);
        hardwareInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;
        DeviceFixture::SetUpImpl(&hardwareInfo);
    }

    void SetUp() override {
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    void checkMMIOs(MMIOList &list, uint32_t tilesNumber, uint32_t localMemorySizeTotalInGB) {
        const uint32_t numberOfTiles = tilesNumber;
        const uint32_t totalLocalMemorySizeGB = localMemorySizeTotalInGB;

        MMIOPair tileAddrRegisters[] = {{0x00004900, 0x0001},
                                        {0x00004904, 0x0001},
                                        {0x00004908, 0x0001},
                                        {0x0000490c, 0x0001}};

        uint32_t localMemoryBase = 0x0;
        for (uint32_t i = 0; i < sizeof(tileAddrRegisters) / sizeof(MMIOPair); i++) {
            tileAddrRegisters[i].second |= localMemoryBase << 1;
            tileAddrRegisters[i].second |= (totalLocalMemorySizeGB / numberOfTiles) << 8;
            localMemoryBase += (totalLocalMemorySizeGB / numberOfTiles);
        }

        uint32_t mmiosFound = 0;
        for (auto &mmioPair : list) {
            for (uint32_t i = 0; i < numberOfTiles; i++) {
                if (mmioPair.first == tileAddrRegisters[i].first && mmioPair.second == tileAddrRegisters[i].second) {
                    mmiosFound++;
                }
            }
        }
        EXPECT_EQ(numberOfTiles, mmiosFound);
    }
};

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTileRangeRegisterTest, givenLocalMemoryEnabledWhenGlobalMmiosAreInitializedThenTileRangeRegistersAreProgrammed) {
    setUpImpl<FamilyType>();
    std::unique_ptr<MockSimulatedCsrHw<FamilyType>> csrSimulatedCommonHw(new MockSimulatedCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    csrSimulatedCommonHw->localMemoryEnabled = true;

    auto stream = std::make_unique<MockAubStreamMockMmioWrite>();
    csrSimulatedCommonHw->stream = stream.get();
    csrSimulatedCommonHw->initGlobalMMIO();

    checkMMIOs(stream->mmioList, 1, 32);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTileRangeRegisterTest, givenLocalMemoryEnabledAnd4TileConfigWhenGlobalMmiosAreInitializedThenTileRangeRegistersAreProgrammed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    setUpImpl<FamilyType>();
    std::unique_ptr<MockSimulatedCsrHw<FamilyType>> csrSimulatedCommonHw(new MockSimulatedCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    csrSimulatedCommonHw->localMemoryEnabled = true;

    auto stream = std::make_unique<MockAubStreamMockMmioWrite>();
    csrSimulatedCommonHw->stream = stream.get();
    csrSimulatedCommonHw->initGlobalMMIO();

    checkMMIOs(stream->mmioList, 4, 32);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTileRangeRegisterTest, givenAUBDumpForceAllToLocalMemoryWhenGlobalMmiosAreInitializedThenTileRangeRegistersAreProgrammed) {
    setUpImpl<FamilyType>();
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    std::unique_ptr<MockSimulatedCsrHw<FamilyType>> csrSimulatedCommonHw(new MockSimulatedCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    csrSimulatedCommonHw->localMemoryEnabled = true;

    auto stream = std::make_unique<MockAubStreamMockMmioWrite>();
    csrSimulatedCommonHw->stream = stream.get();
    csrSimulatedCommonHw->initGlobalMMIO();

    checkMMIOs(stream->mmioList, 1, 32);
}
