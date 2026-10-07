/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"
#include "shared/source/os_interface/device_time_tbx.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include <deque>

using namespace NEO;

namespace {
constexpr uint32_t timestampMmioOffset = 0x1000u;

struct MockAubManagerWithHighDwordSequence : public MockAubManager {
    uint32_t readMMIO(uint32_t offset) override {
        if (offset == timestampMmioOffset + sizeof(uint32_t)) {
            const auto value = highDwordValues.front();
            highDwordValues.pop_front();
            return value;
        }
        return lowDword;
    }

    std::deque<uint32_t> highDwordValues;
    uint32_t lowDword = 0u;
};

struct MockOSTimeWithCpuTime : public OSTime {
    using OSTime::OSTime;
    bool getCpuTime(uint64_t *timeStamp) override {
        *timeStamp = cpuTime;
        return true;
    }
    uint64_t cpuTime = 0x1234u;
};

struct OSTimeTbxTest : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableTimestampMmioRead.set(1);
        aubManager.mmioData = std::unordered_map<uint32_t, uint32_t>{{timestampMmioOffset, 0x89abcdefu}, {timestampMmioOffset + 4, 0x01234567u}};
    }

    DebugManagerStateRestore restorer;
    MockAubManager aubManager;
};
} // namespace

TEST_F(OSTimeTbxTest, givenTimestampMmioReadNotEnabledWhenCreatingThenNullptrIsReturned) {
    debugManager.flags.EnableTimestampMmioRead.set(-1);
    EXPECT_EQ(nullptr, OSTimeTbx::create(aubManager, timestampMmioOffset));

    debugManager.flags.EnableTimestampMmioRead.set(0);
    EXPECT_EQ(nullptr, OSTimeTbx::create(aubManager, timestampMmioOffset));
}

TEST_F(OSTimeTbxTest, givenNoTimestampMmioOffsetWhenCreatingThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, OSTimeTbx::create(aubManager, std::nullopt));
}

TEST_F(OSTimeTbxTest, givenZeroTimestampReadFromMmioWhenCreatingThenNullptrIsReturned) {
    aubManager.mmioData = std::unordered_map<uint32_t, uint32_t>{};
    EXPECT_EQ(nullptr, OSTimeTbx::create(aubManager, timestampMmioOffset));
}

TEST_F(OSTimeTbxTest, givenNonZeroTimestampReadFromMmioWhenCreatingThenOsTimeReadsTimestampFromMmio) {
    auto osTime = OSTimeTbx::create(aubManager, timestampMmioOffset);
    ASSERT_NE(nullptr, osTime);
    EXPECT_TRUE(osTime->isTimestampMmioReadAvailable());

    TimeStampData timestamp{};
    EXPECT_EQ(TimeQueryStatus::success, osTime->getGpuCpuTime(&timestamp));
    EXPECT_EQ(0x0123456789abcdefull, timestamp.gpuTimeStamp);
    EXPECT_NE(0u, timestamp.cpuTimeinNS);
}

TEST_F(OSTimeTbxTest, whenGettingCpuTimeThenHostTimeIsReturned) {
    OSTimeTbx osTime(std::make_unique<DeviceTime>());
    uint64_t cpuTime = 0u;

    EXPECT_TRUE(osTime.getCpuTime(&cpuTime));
    EXPECT_NE(0u, cpuTime);
}

TEST_F(OSTimeTbxTest, whenGettingGpuCpuTimeThenGpuTimestampIsComposedFromLowAndHighDwordAndCpuTimeIsTakenFromOsTime) {
    DeviceTimeTbx deviceTime(aubManager, timestampMmioOffset);
    MockOSTimeWithCpuTime osTime(std::make_unique<DeviceTime>());
    TimeStampData timestamp{};

    EXPECT_EQ(TimeQueryStatus::success, deviceTime.getGpuCpuTimeImpl(&timestamp, &osTime));
    EXPECT_EQ(0x0123456789abcdefull, timestamp.gpuTimeStamp);
    EXPECT_EQ(osTime.cpuTime, timestamp.cpuTimeinNS);
}

TEST_F(OSTimeTbxTest, givenHighDwordChangedDuringReadWhenGettingGpuCpuTimeThenReadIsRepeated) {
    MockAubManagerWithHighDwordSequence sequenceAubManager;
    sequenceAubManager.lowDword = 0x10u;
    sequenceAubManager.highDwordValues = {1u, 2u, 2u, 2u};

    DeviceTimeTbx deviceTime(sequenceAubManager, timestampMmioOffset);
    MockOSTimeWithCpuTime osTime(std::make_unique<DeviceTime>());
    TimeStampData timestamp{};

    EXPECT_EQ(TimeQueryStatus::success, deviceTime.getGpuCpuTimeImpl(&timestamp, &osTime));
    EXPECT_EQ((2ull << 32) | 0x10u, timestamp.gpuTimeStamp);
    EXPECT_TRUE(sequenceAubManager.highDwordValues.empty());
}

TEST_F(OSTimeTbxTest, whenCheckingTimestampsRefreshThenItIsDisabledUnlessEnabledByDebugFlag) {
    DeviceTimeTbx deviceTime(aubManager, timestampMmioOffset);
    EXPECT_FALSE(deviceTime.isTimestampsRefreshEnabled());

    debugManager.flags.EnableReusingGpuTimestamps.set(1);
    EXPECT_TRUE(deviceTime.isTimestampsRefreshEnabled());
}

TEST(RootDeviceEnvironmentTbxOsTimeTest, givenTbxModeAndNoAubCenterWhenInitializingOsTimeThenOsAgnosticOsTimeIsCreated) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableTimestampMmioRead.set(1);
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));

    MockExecutionEnvironment executionEnvironment;
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->aubCenter.reset();

    rootDeviceEnvironment->initOsTime();
    ASSERT_NE(nullptr, rootDeviceEnvironment->osTime);
    EXPECT_FALSE(rootDeviceEnvironment->osTime->isTimestampMmioReadAvailable());
}
