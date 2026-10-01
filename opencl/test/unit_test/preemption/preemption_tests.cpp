/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

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

HWTEST_F(MidThreadPreemptionEnqueueKernelTest, givenKernelAllowingMidThreadPreemptionWhenEnqueuingKernelThenHasMidThreadPreemptionKernelIsSetInDispatchFlags) {
    auto &csr = static_cast<UltCommandStreamReceiver<FamilyType> &>(pCmdQ->getGpgpuCommandStreamReceiver());
    auto &kernelAttributes = const_cast<KernelInfo &>(pKernel->getKernelInfo()).kernelDescriptor.kernelAttributes;
    kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;

    EXPECT_EQ(CL_SUCCESS, callOneWorkItemNDRKernel());

    EXPECT_EQ(PreemptionMode::MidThread, csr.recordedDispatchFlags.preemptionMode);
    EXPECT_TRUE(csr.recordedDispatchFlags.hasMidThreadPreemptionKernel);
}

HWTEST_F(MidThreadPreemptionEnqueueKernelTest, givenKernelRequiringDisabledMidThreadPreemptionWhenEnqueuingKernelThenHasMidThreadPreemptionKernelIsNotSetInDispatchFlags) {
    auto &csr = static_cast<UltCommandStreamReceiver<FamilyType> &>(pCmdQ->getGpgpuCommandStreamReceiver());
    auto &kernelAttributes = const_cast<KernelInfo &>(pKernel->getKernelInfo()).kernelDescriptor.kernelAttributes;
    kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;

    EXPECT_EQ(CL_SUCCESS, callOneWorkItemNDRKernel());
    kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;

    EXPECT_NE(PreemptionMode::MidThread, csr.recordedDispatchFlags.preemptionMode);
    EXPECT_FALSE(csr.recordedDispatchFlags.hasMidThreadPreemptionKernel);
}
