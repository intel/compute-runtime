/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"

#include "opencl/test/unit_test/fixtures/cl_preemption_fixture.h"

#include "gmock/gmock.h"

using namespace NEO;
class ThreadGroupPreemptionTests : public DevicePreemptionTests {
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));
        preemptionMode = PreemptionMode::ThreadGroup;
        DevicePreemptionTests::SetUp();
    }
};

class MidThreadPreemptionTests : public DevicePreemptionTests {
  public:
    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));
        preemptionMode = PreemptionMode::MidThread;
        DevicePreemptionTests::SetUp();
    }
};

TEST_F(ThreadGroupPreemptionTests, GivenDisallowedByKmdThenThreadGroupPreemptionIsDisabled) {
    PreemptionFlags flags = {};
    waTable->waDisablePerCtxtPreemptionGranularityControl = 1;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisallowByDeviceThenThreadGroupPreemptionIsDisabled) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidThread, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisallowByReadWriteFencesWaThenThreadGroupPreemptionIsDisabled) {
    PreemptionFlags flags = {};
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages = true;
    waTable->waDisableLSQCROPERFforOCL = 1;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisallowBySchedulerKernelThenThreadGroupPreemptionIsDisabled) {
    PreemptionFlags flags = {};
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device, true));
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisallowByVmeKernelThenThreadGroupPreemptionIsDisabled) {
    PreemptionFlags flags = {};
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDefaultThenThreadGroupPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDefaultModeForNonKernelRequestThenThreadGroupPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), nullptr);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithEnvironmentPatchSetWhenLSQCWaIsTurnedOnThenThreadGroupPreemptionIsBeingSelected) {
    PreemptionFlags flags = {};
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages = false;
    waTable->waDisableLSQCROPERFforOCL = 1;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithEnvironmentPatchSetWhenLSQCWaIsTurnedOffThenThreadGroupPreemptionIsBeingSelected) {
    PreemptionFlags flags = {};
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages = true;
    waTable->waDisableLSQCROPERFforOCL = 0;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDefaultThenMidBatchPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidBatch);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), nullptr);
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisabledThenPreemptionIsDisabled) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::Disabled);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), nullptr);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenZeroSizedMdiThenThreadGroupPreemptioIsEnabled) {
    MultiDispatchInfo multiDispatchInfo;
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, GivenValidKernelsInMdiThenThreadGroupPreemptioIsEnabled) {
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, GivenValidKernelsInMdiAndDisabledPremptionThenPreemptionIsDisabled) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, GivenAtLeastOneInvalidKernelInMdiThenPreemptionIsDisabled) {
    MockKernel schedulerKernel(program.get(), *kernelInfo, *device, true);
    DispatchInfo schedulerDispatchInfo(device.get(), &schedulerKernel, 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0));

    PreemptionFlags flags = {};
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), &schedulerKernel);
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(schedulerDispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);

    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(MidThreadPreemptionTests, GivenMidThreadPreemptionThenMidThreadPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenNullKernelThenMidThreadPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), nullptr);
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenMidThreadPreemptionDeviceSupportPreemptionOnVmeKernelThenMidThreadPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = true;
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenDisallowMidThreadPreemptionByDeviceThenMidThreadPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(MidThreadPreemptionTests, GivenDisallowMidThreadPreemptionByKernelThenMidThreadPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenDisallowMidThreadPreemptionByVmeKernelThenMidThreadPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = false;
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenTaskPreemptionDisallowMidThreadByDeviceThenThreadGroupPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenTaskPreemptionDisallowMidThreadByKernelThenThreadGroupPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenTaskPreemptionDisallowMidThreadByVmeKernelThenThreadGroupPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = false;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    //VME disables mid thread and thread group when device does not support it
    EXPECT_EQ(PreemptionMode::MidBatch, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenDeviceSupportsMidThreadPreemptionThenMidThreadPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenTaskPreemptionAllowDeviceSupportsPreemptionOnVmeKernelThenMidThreadPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = true;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(ThreadGroupPreemptionTests, GivenDebugKernelPreemptionWhenDeviceSupportsThreadGroupThenExpectDebugKeyMidThreadValue) {
    DebugManager.flags.ForceKernelPreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

    EXPECT_EQ(PreemptionMode::ThreadGroup, device->getPreemptionMode());

    PreemptionFlags flags = {};
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenDebugKernelPreemptionWhenDeviceSupportsMidThreadThenExpectDebugKeyMidBatchValue) {
    DebugManager.flags.ForceKernelPreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidBatch));

    EXPECT_EQ(PreemptionMode::MidThread, device->getPreemptionMode());

    PreemptionFlags flags = {};
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidBatch, outMode);
}
