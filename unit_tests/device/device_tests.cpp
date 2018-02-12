/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/options.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"
#include <memory>
#include "runtime/device/device.h"

using namespace OCLRT;
struct DeviceTest : public DeviceFixture,
                    public MemoryManagementFixture,
                    public ::testing::Test {
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
        MemoryManagementFixture::TearDown();
    }
};

TEST_F(DeviceTest, Create) {
    EXPECT_NE(nullptr, pDevice);
}

TEST_F(DeviceTest, getCommandStreamReceiver) {
    EXPECT_NE(nullptr, &pDevice->getCommandStreamReceiver());
}

TEST_F(DeviceTest, givenDeviceWhenPeekCommandStreamReceiverIsCalledThenCommandStreamReceiverIsReturned) {
    EXPECT_NE(nullptr, pDevice->peekCommandStreamReceiver());
}

TEST_F(DeviceTest, getSupportedClVersion) {
    auto version = pDevice->getSupportedClVersion();
    auto version2 = pDevice->getHardwareInfo().capabilityTable.clVersionSupport;

    EXPECT_EQ(version, version2);
}

TEST_F(DeviceTest, getTagAddress) {
    auto pTagAddress = pDevice->getTagAddress();
    ASSERT_NE(nullptr, const_cast<uint32_t *>(pTagAddress));
    EXPECT_EQ(initialHardwareTag, *pTagAddress);
}

TEST_F(DeviceTest, WhenGetOSTimeThenNotNull) {
    auto pDevice = std::unique_ptr<Device>(DeviceHelper<>::create());

    OSTime *osTime = pDevice->getOSTime();
    ASSERT_NE(nullptr, osTime);
}

TEST_F(DeviceTest, GivenDebugVariableForcing32BitAllocationsWhenDeviceIsCreatedThenMemoryManagerHasForce32BitFlagSet) {
    DebugManager.flags.Force32bitAddressing.set(true);
    auto pDevice = std::unique_ptr<Device>(DeviceHelper<>::create());
    if (is64bit) {
        EXPECT_TRUE(pDevice->getDeviceInfo().force32BitAddressess);
        EXPECT_TRUE(pDevice->getMemoryManager()->peekForce32BitAllocations());
    } else {
        EXPECT_FALSE(pDevice->getDeviceInfo().force32BitAddressess);
        EXPECT_FALSE(pDevice->getMemoryManager()->peekForce32BitAllocations());
    }
    DebugManager.flags.Force32bitAddressing.set(false);
}

TEST_F(DeviceTest, retainAndRelease) {
    ASSERT_NE(nullptr, pDevice);
    ASSERT_EQ(true, pDevice->isRootDevice());

    pDevice->retain();
    pDevice->retain();
    pDevice->retain();
    ASSERT_EQ(1, pDevice->getReference());

    ASSERT_FALSE(pDevice->release().isUnused());
    ASSERT_EQ(1, pDevice->getReference());
}

TEST_F(DeviceTest, givenMemoryManagerWhenDeviceIsCreatedThenItHasAccessToDevice) {
    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_EQ(memoryManager->device, pDevice);
}

TEST_F(DeviceTest, getEngineTypeDefault) {
    auto pTestDevice = std::unique_ptr<Device>(createWithUsDeviceId(0));

    EngineType actualEngineType = pDevice->getEngineType();
    EngineType defaultEngineType = hwInfoHelper.capabilityTable.defaultEngineType;

    EXPECT_EQ(defaultEngineType, actualEngineType);
}

TEST_F(DeviceTest, givenDebugVariableOverrideEngineTypeWhenDeviceIsCreatedThenUseDebugNotDefaul) {
    EngineType expectedEngine = EngineType::ENGINE_VCS;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(expectedEngine));
    auto pTestDevice = std::unique_ptr<Device>(createWithUsDeviceId(0));

    EngineType actualEngineType = pTestDevice->getEngineType();
    EngineType defaultEngineType = hwInfoHelper.capabilityTable.defaultEngineType;

    EXPECT_NE(defaultEngineType, actualEngineType);
    EXPECT_EQ(expectedEngine, actualEngineType);
}

struct SmallMockDevice : public Device {
    SmallMockDevice(const HardwareInfo &hwInfo, bool isRootDevice = true)
        : Device(hwInfo, isRootDevice) {}
    GraphicsAllocation *peekTagAllocation() { return this->tagAllocation; }
};

TEST(DeviceCreation, givenDeviceWithUsedTagAllocationWhenItIsDestroyedThenThereAreNoCrashesAndLeaks) {
    overrideCommandStreamReceiverCreation = 1;
    std::unique_ptr<SmallMockDevice> device(Device::create<SmallMockDevice>(platformDevices[0]));
    auto tagAllocation = device->peekTagAllocation();
    tagAllocation->taskCount = 1;
}
