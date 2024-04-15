/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_preemption_helper.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/fixtures/cl_preemption_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;
class ThreadGroupPreemptionTests : public DevicePreemptionTests {
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));
        preemptionMode = PreemptionMode::ThreadGroup;
        DevicePreemptionTests::SetUp();
    }
};

class MidThreadPreemptionTests : public DevicePreemptionTests {
  public:
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));
        preemptionMode = PreemptionMode::MidThread;
        DevicePreemptionTests::SetUp();
    }
};

TEST_F(ThreadGroupPreemptionTests, GivenZeroSizedMdiThenThreadGroupPreemptioIsEnabled) {
    MultiDispatchInfo multiDispatchInfo;
    EXPECT_EQ(PreemptionMode::ThreadGroup, ClPreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, GivenValidKernelsInMdiThenThreadGroupPreemptioIsEnabled) {
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::ThreadGroup, ClPreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, GivenValidKernelsInMdiAndDisabledPreemptionThenPreemptionIsDisabled) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::Disabled, ClPreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(MidThreadPreemptionTests, GivenMultiDispatchWithoutKernelWhenDevicePreemptionIsMidThreadThenTaskPreemptionIsMidThread) {
    dispatchInfo.reset(new DispatchInfo(device.get(), nullptr, 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0)));

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);

    EXPECT_EQ(PreemptionMode::MidThread, ClPreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

HWTEST_F(MidThreadPreemptionTests, GivenValueArgWhenOverrideMidThreadPreemptionSupportThenNothingChange) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    bool value = true;
    ClPreemptionHelper::overrideMidThreadPreemptionSupport(*context.get(), value);
    EXPECT_EQ(PreemptionMode::MidThread, device->getPreemptionMode());
    value = false;
    ClPreemptionHelper::overrideMidThreadPreemptionSupport(*context.get(), value);
    EXPECT_EQ(PreemptionMode::MidThread, device->getPreemptionMode());

    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    value = true;
    ClPreemptionHelper::overrideMidThreadPreemptionSupport(*context.get(), value);
    EXPECT_EQ(PreemptionMode::ThreadGroup, device->getPreemptionMode());
    value = false;
    ClPreemptionHelper::overrideMidThreadPreemptionSupport(*context.get(), value);
    EXPECT_EQ(PreemptionMode::ThreadGroup, device->getPreemptionMode());
}
