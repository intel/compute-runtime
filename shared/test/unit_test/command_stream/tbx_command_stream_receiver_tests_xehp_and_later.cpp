/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/memory_manager/physical_address_allocator.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct XeHPAndLaterTbxCommandStreamReceiverTests : DeviceFixture, ::testing::Test {
    template <typename FamilyType>
    void setUpImpl() {
        hardwareInfo = *defaultHwInfo;
        hardwareInfoSetup[hardwareInfo.platform.eProductFamily](&hardwareInfo, true, 0);
        hardwareInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;
        DeviceFixture::setUpImpl(&hardwareInfo);
    }

    void SetUp() override {
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }
};

template <typename FamilyType>
struct MockTbxCommandStreamReceiverHw : TbxCommandStreamReceiverHw<FamilyType> {
    using TbxCommandStreamReceiverHw<FamilyType>::TbxCommandStreamReceiverHw;

    uint32_t getDeviceIndex() const override {
        return deviceIndex;
    }

    uint32_t deviceIndex = 0u;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTbxCommandStreamReceiverTests, givenNullPtrGraphicsAlloctionWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    setUpImpl<FamilyType>();
    auto tbxCsr = std::make_unique<MockTbxCommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    GraphicsAllocation *allocation = nullptr;
    auto bits = tbxCsr->getPPGTTAdditionalBits(allocation);

    EXPECT_EQ(3u, bits);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTbxCommandStreamReceiverTests, givenGraphicsAlloctionWithNonLocalMemoryPoolWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    setUpImpl<FamilyType>();
    auto tbxCsr = std::make_unique<MockTbxCommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockGraphicsAllocation allocation(nullptr, 0);
    auto bits = tbxCsr->getPPGTTAdditionalBits(&allocation);

    EXPECT_EQ(3u, bits);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTbxCommandStreamReceiverTests, givenGraphicsAlloctionWithLocalMemoryPoolWhenGetPPGTTAdditionalBitsIsCalledThenAppropriateValueIsReturned) {
    setUpImpl<FamilyType>();
    auto tbxCsr = std::make_unique<MockTbxCommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockGraphicsAllocation allocation(nullptr, 0);
    allocation.overrideMemoryPool(MemoryPool::LocalMemory);
    auto bits = tbxCsr->getPPGTTAdditionalBits(&allocation);

    EXPECT_EQ(3u | (1 << 11), bits);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTbxCommandStreamReceiverTests, givenAubDumpForceAllToLocalMemoryPoolWhenGetPPGTTAdditionalBitsIsCalledThenLocalBitIsReturned) {
    setUpImpl<FamilyType>();
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    auto tbxCsr = std::make_unique<MockTbxCommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockGraphicsAllocation allocation(nullptr, 0);

    auto bits = tbxCsr->getPPGTTAdditionalBits(&allocation);

    EXPECT_EQ(3u | (1 << 11), bits);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTbxCommandStreamReceiverTests, givenLocalMemoryFeatureWhenGetGTTDataIsCalledThenLocalMemoryIsSet) {
    setUpImpl<FamilyType>();
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo));
    auto tbxCsr = std::make_unique<MockTbxCommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    tbxCsr->setupContext(*device->getDefaultEngine().osContext);

    AubGTTData data = {false, false};
    tbxCsr->getGTTData(nullptr, data);
    EXPECT_TRUE(data.localMemory);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTbxCommandStreamReceiverTests, givenLocalMemoryEnabledWhenGetMemoryBankForGttIsCalledThenCorrectBankForDeviceIsReturned) {
    setUpImpl<FamilyType>();
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo));
    auto tbxCsr = std::make_unique<MockTbxCommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());

    auto bank = tbxCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::getBankForLocalMemory(0), bank);

    tbxCsr->deviceIndex = 1u;
    bank = tbxCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::getBankForLocalMemory(1), bank);

    tbxCsr->deviceIndex = 2u;
    bank = tbxCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::getBankForLocalMemory(2), bank);

    tbxCsr->deviceIndex = 3u;
    bank = tbxCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::getBankForLocalMemory(3), bank);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTbxCommandStreamReceiverTests, whenPhysicalAllocatorIsCreatedThenItHasCorrectBankSzieAndNumberOfBanks) {
    setUpImpl<FamilyType>();
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo));
    auto tbxCsr = std::make_unique<MockTbxCommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    auto physicalAddressAllocator = tbxCsr->physicalAddressAllocator.get();
    auto allocator = reinterpret_cast<PhysicalAddressAllocatorHw<FamilyType> *>(physicalAddressAllocator);

    EXPECT_EQ(32 * MemoryConstants::gigaByte, allocator->getBankSize());
    EXPECT_EQ(1u, allocator->getNumberOfBanks());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTbxCommandStreamReceiverTests, whenPhysicalAllocatorIsCreatedFor4TilesThenItHasCorrectBankSzieAndNumberOfBanks) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    setUpImpl<FamilyType>();
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo));
    auto tbxCsr = std::make_unique<MockTbxCommandStreamReceiverHw<FamilyType>>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    auto physicalAddressAllocator = tbxCsr->physicalAddressAllocator.get();
    auto allocator = reinterpret_cast<PhysicalAddressAllocatorHw<FamilyType> *>(physicalAddressAllocator);

    EXPECT_EQ(8 * MemoryConstants::gigaByte, allocator->getBankSize());
    EXPECT_EQ(4u, allocator->getNumberOfBanks());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterTbxCommandStreamReceiverTests, whenAskedForPollForCompletionParametersThenReturnCorrectValues) {
    setUpImpl<FamilyType>();
    class MyMockTbxHw : public TbxCommandStreamReceiverHw<FamilyType> {
      public:
        MyMockTbxHw(ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
            : TbxCommandStreamReceiverHw<FamilyType>(executionEnvironment, 0, deviceBitfield) {}
        using TbxCommandStreamReceiverHw<FamilyType>::getpollNotEqualValueForPollForCompletion;
        using TbxCommandStreamReceiverHw<FamilyType>::getMaskAndValueForPollForCompletion;
    };

    MyMockTbxHw myMockTbxHw(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    EXPECT_EQ(0x80u, myMockTbxHw.getMaskAndValueForPollForCompletion());
    EXPECT_TRUE(myMockTbxHw.getpollNotEqualValueForPollForCompletion());
}
