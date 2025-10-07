/*
 * Copyright (C) 2021-2025 Intel Corporation
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
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_aub_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <set>
#include <vector>

using namespace NEO;

struct XeHPAndLaterAubCommandStreamReceiverTests : DeviceFixture, ::testing::Test {
    template <typename FamilyType>
    void setUpImpl() {
        hardwareInfo = *defaultHwInfo;
        auto releaseHelper = ReleaseHelper::create(hardwareInfo.ipVersion);
        hardwareInfoSetup[hardwareInfo.platform.eProductFamily](&hardwareInfo, true, 0, releaseHelper.get());
        hardwareInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;
        DeviceFixture::setUpImpl(&hardwareInfo);
    }

    void SetUp() override {
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
};

template <typename FamilyType>
class MockAubCsrXeHPAndLater : public AUBCommandStreamReceiverHw<FamilyType> {
  public:
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

    MockOsContext rcsOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    MockOsContext ccs0OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    MockOsContext ccs1OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS1, EngineUsage::regular}));
    MockOsContext ccs2OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS2, EngineUsage::regular}));
    MockOsContext ccs3OsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS3, EngineUsage::regular}));
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
    debugManager.flags.AUBDumpForceAllToLocalMemory.set(false);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    MockGraphicsAllocation allocation(nullptr, 0);
    auto bits = aubCsr->getPPGTTAdditionalBits(&allocation);

    EXPECT_EQ(3u, bits);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenGraphicsAlloctionWithLocalMemoryPoolWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    setUpImpl<FamilyType>();

    DebugManagerStateRestore debugRestorer;
    debugManager.flags.AUBDumpForceAllToLocalMemory.set(false);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    MockGraphicsAllocation allocation(nullptr, 0);
    allocation.overrideMemoryPool(MemoryPool::localMemory);
    auto bits = aubCsr->getPPGTTAdditionalBits(&allocation);

    EXPECT_EQ(3u | (1 << 11), bits);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, givenAubDumpForceAllToLocalMemoryPoolWhenGetPPGTTAdditionalBitsIsCalledThenLocalBitIsReturned) {
    setUpImpl<FamilyType>();

    DebugManagerStateRestore debugRestorer;
    debugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    MockGraphicsAllocation allocation(nullptr, 0);

    auto bits = aubCsr->getPPGTTAdditionalBits(&allocation);

    EXPECT_EQ(3u | (1 << 11), bits);
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

    auto physicalAddressAllocator = std::unique_ptr<PhysicalAddressAllocator>(aubCsr->createPhysicalAddressAllocator(&pDevice->getHardwareInfo(), pDevice->getReleaseHelper()));
    auto allocator = reinterpret_cast<PhysicalAddressAllocatorHw<FamilyType> *>(physicalAddressAllocator.get());
    auto expectedBankSize = AubHelper::getTotalMemBankSize(pDevice->getReleaseHelper());

    EXPECT_EQ(expectedBankSize, allocator->getBankSize());
    EXPECT_EQ(1u, allocator->getNumberOfBanks());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests, whenPhysicalAllocatorIsCreatedWith4TileConfigThenItHasCorrectBankSizeAndNumberOfBanks) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(4);
    setUpImpl<FamilyType>();

    std::unique_ptr<MockAubCsrXeHPAndLater<FamilyType>> aubCsr(new MockAubCsrXeHPAndLater<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    auto physicalAddressAllocator = std::unique_ptr<PhysicalAddressAllocator>(aubCsr->createPhysicalAddressAllocator(&pDevice->getHardwareInfo(), pDevice->getReleaseHelper()));
    auto allocator = reinterpret_cast<PhysicalAddressAllocatorHw<FamilyType> *>(physicalAddressAllocator.get());
    auto expectedBankSize = AubHelper::getTotalMemBankSize(pDevice->getReleaseHelper()) / 4;

    EXPECT_EQ(expectedBankSize, allocator->getBankSize());
    EXPECT_EQ(4u, allocator->getNumberOfBanks());
}

using XeHPAndLaterAubCommandStreamReceiverTests2 = GfxCoreHelperTest;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubCommandStreamReceiverTests2, givenLocalMemoryEnabledInCSRWhenGetGTTDataIsCalledThenLocalMemoryIsSet) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.EnableLocalMemory.set(1);
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo));
    std::unique_ptr<MockAubCsrXeHPAndLater<FamilyType>> aubCsr(std::make_unique<MockAubCsrXeHPAndLater<FamilyType>>("", true, *device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield()));
    EXPECT_TRUE(aubCsr->localMemoryEnabled);

    AubGTTData data = {false, false};
    aubCsr->getGTTData(nullptr, data);
    EXPECT_TRUE(data.localMemory);
}
