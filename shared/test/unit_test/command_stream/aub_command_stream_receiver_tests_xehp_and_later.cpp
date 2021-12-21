/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_aub_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/test.h"

#include <set>
#include <vector>

using namespace NEO;

struct XeHPAndLaterAubCommandStreamReceiverTests : DeviceFixture, ::testing::Test {
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
};

template <typename FamilyType>
class MockAubCsrXeHPAndLater : public AUBCommandStreamReceiverHw<FamilyType> {
  public:
    using AUBCommandStreamReceiverHw<FamilyType>::getAddressSpace;
    using CommandStreamReceiverHw<FamilyType>::localMemoryEnabled;
    using CommandStreamReceiverSimulatedHw<FamilyType>::createPhysicalAddressAllocator;

    MockAubCsrXeHPAndLater(const std::string &fileName,
                           bool standalone, ExecutionEnvironment &executionEnvironment,
                           uint32_t rootDeviceIndex,
                           const DeviceBitfield deviceBitfield)
        : AUBCommandStreamReceiverHw<FamilyType>(fileName, standalone, executionEnvironment, rootDeviceIndex, deviceBitfield) {}

    uint32_t getDeviceIndex() const override {
        return deviceIndex;
    }

    uint32_t deviceIndex = 0u;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetGUCWorkQueueItemHeaderIsCalledThenAppropriateValueDependingOnEngineTypeIsReturned) {
    setUpImpl<FamilyType>();

    MockOsContext rcsOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular}));
    MockOsContext ccs0OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular}));
    MockOsContext ccs1OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS1, EngineUsage::Regular}));
    MockOsContext ccs2OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS2, EngineUsage::Regular}));
    MockOsContext ccs3OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS3, EngineUsage::Regular}));
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    aubCsr->setupContext(ccs0OsContext);
    EXPECT_EQ(0x00030401u, aubCsr->getGUCWorkQueueItemHeader());
    aubCsr->setupContext(ccs1OsContext);
    EXPECT_EQ(0x00030401u, aubCsr->getGUCWorkQueueItemHeader());
    aubCsr->setupContext(ccs2OsContext);
    EXPECT_EQ(0x00030401u, aubCsr->getGUCWorkQueueItemHeader());
    aubCsr->setupContext(ccs3OsContext);
    EXPECT_EQ(0x00030401u, aubCsr->getGUCWorkQueueItemHeader());
    aubCsr->setupContext(rcsOsContext);
    EXPECT_EQ(0x00030001u, aubCsr->getGUCWorkQueueItemHeader());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenGraphicsAlloctionWithNonLocalMemoryPoolWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    setUpImpl<FamilyType>();

    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(false);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    MockGraphicsAllocation allocation(nullptr, 0);
    auto bits = aubCsr->getPPGTTAdditionalBits(&allocation);

    EXPECT_EQ(3u, bits);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenGraphicsAlloctionWithLocalMemoryPoolWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    setUpImpl<FamilyType>();

    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(false);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    MockGraphicsAllocation allocation(nullptr, 0);
    allocation.overrideMemoryPool(MemoryPool::LocalMemory);
    auto bits = aubCsr->getPPGTTAdditionalBits(&allocation);

    EXPECT_EQ(3u | (1 << 11), bits);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenAubDumpForceAllToLocalMemoryPoolWhenGetPPGTTAdditionalBitsIsCalledThenLocalBitIsReturned) {
    setUpImpl<FamilyType>();

    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    MockGraphicsAllocation allocation(nullptr, 0);

    auto bits = aubCsr->getPPGTTAdditionalBits(&allocation);

    EXPECT_EQ(3u | (1 << 11), bits);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenAubDumpForceAllToLocalMemoryEnabledWhenGetAddressSpaceIsCalledThenTraceLocalIsReturned) {
    setUpImpl<FamilyType>();

    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    std::unique_ptr<MockAubCsrXeHPAndLater<FamilyType>> aubCsr(new MockAubCsrXeHPAndLater<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();

    auto addressSpace = aubCsr->getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype);

    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, addressSpace);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenAubDumpForceAllToLocalMemoryDisabledWhenGetAddressSpaceIsCalledThenTraceNonlocalIsReturned) {
    setUpImpl<FamilyType>();

    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(false);

    std::unique_ptr<MockAubCsrXeHPAndLater<FamilyType>> aubCsr(new MockAubCsrXeHPAndLater<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();

    auto addressSpace = aubCsr->getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype);

    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, addressSpace);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenCCSEnabledWhenEngineMmiosAreInitializedThenExpectL3ConfigMmioIsWritten) {
    setUpImpl<FamilyType>();

    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular}));
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    aubCsr.setupContext(osContext);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr.stream = stream.get();

    aubCsr.initEngineMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0xB234, 0xA0000000u)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenRCSEnabledWhenEngineMmiosAreInitializedThenExpectL3ConfigMmioIsWritten) {
    setUpImpl<FamilyType>();

    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    AUBCommandStreamReceiverHw<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    aubCsr.setupContext(osContext);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr.stream = stream.get();

    aubCsr.initEngineMMIO();

    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0xB134, 0xA0000000u)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenLocaLMemoryBitWhenGetAddressSpaceFromPTEBitsIsCalledThenTraceLocalIsReturned) {
    setUpImpl<FamilyType>();

    std::unique_ptr<MockAubCsrXeHPAndLater<FamilyType>> aubCsr(new MockAubCsrXeHPAndLater<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();

    uint64_t bits = BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) | BIT(PageTableEntry::localMemoryBit);
    auto addressSpace = aubCsr->getAddressSpaceFromPTEBits(bits);
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, addressSpace);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenLocalMemoryEnabledWhenGetMemoryBankForGttIsCalledThenCorrectBankForDeviceIsReturned) {
    setUpImpl<FamilyType>();

    std::unique_ptr<MockAubCsrXeHPAndLater<FamilyType>> aubCsr(new MockAubCsrXeHPAndLater<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    aubCsr->localMemoryEnabled = true;

    auto bank = aubCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::getBankForLocalMemory(0), bank);

    aubCsr->deviceIndex = 1u;
    bank = aubCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::getBankForLocalMemory(1), bank);

    aubCsr->deviceIndex = 2u;
    bank = aubCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::getBankForLocalMemory(2), bank);

    aubCsr->deviceIndex = 3u;
    bank = aubCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::getBankForLocalMemory(3), bank);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, whenPhysicalAllocatorIsCreatedThenItHasCorrectBankSzieAndNumberOfBanks) {
    setUpImpl<FamilyType>();

    std::unique_ptr<MockAubCsrXeHPAndLater<FamilyType>> aubCsr(new MockAubCsrXeHPAndLater<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    auto physicalAddressAllocator = std::unique_ptr<PhysicalAddressAllocator>(aubCsr->createPhysicalAddressAllocator(&pDevice->getHardwareInfo()));
    auto allocator = reinterpret_cast<PhysicalAddressAllocatorHw<FamilyType> *>(physicalAddressAllocator.get());

    EXPECT_EQ(32 * MemoryConstants::gigaByte, allocator->getBankSize());
    EXPECT_EQ(1u, allocator->getNumberOfBanks());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, whenPhysicalAllocatorIsCreatedWith4TileConfigThenItHasCorrectBankSzieAndNumberOfBanks) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    setUpImpl<FamilyType>();

    std::unique_ptr<MockAubCsrXeHPAndLater<FamilyType>> aubCsr(new MockAubCsrXeHPAndLater<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    auto physicalAddressAllocator = std::unique_ptr<PhysicalAddressAllocator>(aubCsr->createPhysicalAddressAllocator(&pDevice->getHardwareInfo()));
    auto allocator = reinterpret_cast<PhysicalAddressAllocatorHw<FamilyType> *>(physicalAddressAllocator.get());

    EXPECT_EQ(8 * MemoryConstants::gigaByte, allocator->getBankSize());
    EXPECT_EQ(4u, allocator->getNumberOfBanks());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenInitEngineMMIOIsCalledForGivenEngineTypeThenCorrespondingMmiosAreInitialized) {
    setUpImpl<FamilyType>();

    DebugManagerStateRestore debugRestorer;
    MockOsContext rcsOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular}));
    MockOsContext ccs0OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular}));
    MockOsContext ccs1OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS1, EngineUsage::Regular}));
    MockOsContext ccs2OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS2, EngineUsage::Regular}));
    MockOsContext ccs3OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS3, EngineUsage::Regular}));

    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();

    aubCsr->setupContext(rcsOsContext);
    aubCsr->initEngineMMIO();
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0002000 + 0x000058, 0x00000000)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0002000 + 0x00029c, 0xffff8280)));

    aubCsr->setupContext(ccs0OsContext);
    aubCsr->initEngineMMIO();
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000ce90, 0x00030003)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x1a000 + 0x000029c, 0xffff8280)));

    aubCsr->setupContext(ccs1OsContext);
    aubCsr->initEngineMMIO();
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000ce90, 0x00030003)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x1c000 + 0x000029c, 0xffff8280)));

    aubCsr->setupContext(ccs2OsContext);
    aubCsr->initEngineMMIO();
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000ce90, 0x00030003)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x1e000 + 0x000029c, 0xffff8280)));

    aubCsr->setupContext(ccs3OsContext);
    aubCsr->initEngineMMIO();
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x0000ce90, 0x00030003)));
    EXPECT_TRUE(stream->isOnMmioList(MMIOPair(0x26000 + 0x000029c, 0xffff8280)));
}

template <class FamilyType>
static void checkCcsEngineMMIO(aub_stream::EngineType engineType, uint32_t mmioBase) {
    auto &mmioList = *AUBFamilyMapper<FamilyType>::perEngineMMIO[engineType];

    EXPECT_EQ(mmioList[0], MMIOPair(0x0000ce90, 0x00030003));
    EXPECT_EQ(mmioList[1], MMIOPair(0x0000b170, 0x00030003));
    EXPECT_EQ(mmioList[2], MMIOPair(0x00014800, 0xFFFF0001));
    EXPECT_EQ(mmioList[3], MMIOPair(mmioBase + 0x000029c, 0xffff8280));

    EXPECT_EQ(mmioList[4], MMIOPair(mmioBase + 0x00004d0, 0x0000e000));
    EXPECT_EQ(mmioList[5], MMIOPair(mmioBase + 0x00004d4, 0x0000e000));
    EXPECT_EQ(mmioList[6], MMIOPair(mmioBase + 0x00004d8, 0x0000e000));
    EXPECT_EQ(mmioList[7], MMIOPair(mmioBase + 0x00004dc, 0x0000e000));
    EXPECT_EQ(mmioList[8], MMIOPair(mmioBase + 0x00004e0, 0x0000e000));
    EXPECT_EQ(mmioList[9], MMIOPair(mmioBase + 0x00004e4, 0x0000e000));
    EXPECT_EQ(mmioList[10], MMIOPair(mmioBase + 0x00004e8, 0x0000e000));
    EXPECT_EQ(mmioList[11], MMIOPair(mmioBase + 0x00004ec, 0x0000e000));
    EXPECT_EQ(mmioList[12], MMIOPair(mmioBase + 0x00004f0, 0x0000e000));
    EXPECT_EQ(mmioList[13], MMIOPair(mmioBase + 0x00004f4, 0x0000e000));
    EXPECT_EQ(mmioList[14], MMIOPair(mmioBase + 0x00004f8, 0x0000e000));
    EXPECT_EQ(mmioList[15], MMIOPair(mmioBase + 0x00004fc, 0x0000e000));

    EXPECT_EQ(mmioList[16], MMIOPair(0x0000B234, 0xA0000000));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenCcsEngineMmioListForSpecificCcsInstanceIsReadThenItIsInitializedWithProperValues) {
    setUpImpl<FamilyType>();

    checkCcsEngineMMIO<FamilyType>(aub_stream::ENGINE_CCS, 0x1a000);
    checkCcsEngineMMIO<FamilyType>(aub_stream::ENGINE_CCS1, 0x1c000);
    checkCcsEngineMMIO<FamilyType>(aub_stream::ENGINE_CCS2, 0x1e000);
    checkCcsEngineMMIO<FamilyType>(aub_stream::ENGINE_CCS3, 0x26000);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenRcsEngineMmioListIsReadThenItIsInitializedWithProperValues) {
    setUpImpl<FamilyType>();

    auto &mmioList = *AUBFamilyMapper<FamilyType>::perEngineMMIO[aub_stream::ENGINE_RCS];
    auto mmioBase = 0x002000;

    EXPECT_EQ(mmioList[0], MMIOPair(mmioBase + 0x000058, 0x00000000));
    EXPECT_EQ(mmioList[1], MMIOPair(mmioBase + 0x0000a8, 0x00000000));
    EXPECT_EQ(mmioList[2], MMIOPair(mmioBase + 0x000029c, 0xffff8280));

    EXPECT_EQ(mmioList[3], MMIOPair(0x00002090, 0xffff0000));
    EXPECT_EQ(mmioList[4], MMIOPair(0x000020e0, 0xffff4000));
    EXPECT_EQ(mmioList[5], MMIOPair(0x000020e4, 0xffff0000));
    EXPECT_EQ(mmioList[6], MMIOPair(0x000020ec, 0xffff0051));

    EXPECT_EQ(mmioList[7], MMIOPair(mmioBase + 0x00004d0, 0x00007014));
    EXPECT_EQ(mmioList[8], MMIOPair(mmioBase + 0x00004d4, 0x0000e000));
    EXPECT_EQ(mmioList[9], MMIOPair(mmioBase + 0x00004d8, 0x0000e000));
    EXPECT_EQ(mmioList[10], MMIOPair(mmioBase + 0x00004dc, 0x0000e000));
    EXPECT_EQ(mmioList[11], MMIOPair(mmioBase + 0x00004e0, 0x0000e000));
    EXPECT_EQ(mmioList[12], MMIOPair(mmioBase + 0x00004e4, 0x0000e000));
    EXPECT_EQ(mmioList[13], MMIOPair(mmioBase + 0x00004e8, 0x0000e000));
    EXPECT_EQ(mmioList[14], MMIOPair(mmioBase + 0x00004ec, 0x0000e000));
    EXPECT_EQ(mmioList[15], MMIOPair(mmioBase + 0x00004f0, 0x0000e000));
    EXPECT_EQ(mmioList[16], MMIOPair(mmioBase + 0x00004f4, 0x0000e000));
    EXPECT_EQ(mmioList[17], MMIOPair(mmioBase + 0x00004f8, 0x0000e000));
    EXPECT_EQ(mmioList[18], MMIOPair(mmioBase + 0x00004fc, 0x0000e000));

    EXPECT_EQ(mmioList[19], MMIOPair(0x00002580, 0xffff0005));
    EXPECT_EQ(mmioList[20], MMIOPair(0x0000e194, 0xffff0002));

    EXPECT_EQ(mmioList[21], MMIOPair(0x0000B134, 0xA0000000));
}

using XeHPAndLaterAubCommandStreamReceiverTests2 = HwHelperTest;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests2, givenLocalMemoryEnabledInCSRWhenGetGTTDataIsCalledThenLocalMemoryIsSet) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo));
    std::unique_ptr<MockAubCsrXeHPAndLater<FamilyType>> aubCsr(std::make_unique<MockAubCsrXeHPAndLater<FamilyType>>("", true, *device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield()));
    EXPECT_TRUE(aubCsr->localMemoryEnabled);

    AubGTTData data = {false, false};
    aubCsr->getGTTData(nullptr, data);
    EXPECT_TRUE(data.localMemory);
}
