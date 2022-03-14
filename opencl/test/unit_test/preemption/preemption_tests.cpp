/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"

#include "opencl/source/helpers/cl_preemption_helper.h"
#include "opencl/test/unit_test/fixtures/cl_preemption_fixture.h"

#include "gtest/gtest.h"

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
    waTable->flags.waDisablePerCtxtPreemptionGranularityControl = 1;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisallowByDeviceThenThreadGroupPreemptionIsDisabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidThread, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisallowByReadWriteFencesWaThenThreadGroupPreemptionIsDisabled) {
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages = true;
    waTable->flags.waDisableLSQCROPERFforOCL = 1;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisallowByVmeKernelThenThreadGroupPreemptionIsDisabled) {
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDefaultThenThreadGroupPreemptionIsEnabled) {
    PreemptionFlags flags = {};
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDefaultModeForNonKernelRequestThenThreadGroupPreemptionIsEnabled) {
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), nullptr);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithEnvironmentPatchSetWhenLSQCWaIsTurnedOnThenThreadGroupPreemptionIsBeingSelected) {
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages = false;
    waTable->flags.waDisableLSQCROPERFforOCL = 1;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithEnvironmentPatchSetWhenLSQCWaIsTurnedOffThenThreadGroupPreemptionIsBeingSelected) {
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages = true;
    waTable->flags.waDisableLSQCROPERFforOCL = 0;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDefaultThenMidBatchPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::MidBatch);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), nullptr);
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, GivenDisabledThenPreemptionIsDisabled) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), nullptr);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

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

TEST_F(ThreadGroupPreemptionTests, GivenValidKernelsInMdiAndDisabledPremptionThenPreemptionIsDisabled) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::Disabled, ClPreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(MidThreadPreemptionTests, GivenMidThreadPreemptionThenMidThreadPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenNullKernelThenMidThreadPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), nullptr);
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenMidThreadPreemptionDeviceSupportPreemptionOnVmeKernelThenMidThreadPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = true;
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenDisallowMidThreadPreemptionByDeviceThenMidThreadPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(MidThreadPreemptionTests, GivenDisallowMidThreadPreemptionByKernelThenMidThreadPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenDisallowMidThreadPreemptionByVmeKernelThenMidThreadPreemptionIsEnabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = false;
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, GivenTaskPreemptionDisallowMidThreadByDeviceThenThreadGroupPreemptionIsEnabled) {
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenTaskPreemptionDisallowMidThreadByKernelThenThreadGroupPreemptionIsEnabled) {
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = true;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenTaskPreemptionDisallowMidThreadByVmeKernelThenThreadGroupPreemptionIsEnabled) {
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = false;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    //VME disables mid thread and thread group when device does not support it
    EXPECT_EQ(PreemptionMode::MidBatch, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenDeviceSupportsMidThreadPreemptionThenMidThreadPreemptionIsEnabled) {
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenTaskPreemptionAllowDeviceSupportsPreemptionOnVmeKernelThenMidThreadPreemptionIsEnabled) {
    kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = false;
    kernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = true;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(ThreadGroupPreemptionTests, GivenDebugKernelPreemptionWhenDeviceSupportsThreadGroupThenExpectDebugKeyMidThreadValue) {
    DebugManager.flags.ForceKernelPreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

    EXPECT_EQ(PreemptionMode::ThreadGroup, device->getPreemptionMode());

    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenDebugKernelPreemptionWhenDeviceSupportsMidThreadThenExpectDebugKeyMidBatchValue) {
    DebugManager.flags.ForceKernelPreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidBatch));

    EXPECT_EQ(PreemptionMode::MidThread, device->getPreemptionMode());

    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device->getDevice(), &kernel->getDescriptor());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidBatch, outMode);
}

TEST_F(MidThreadPreemptionTests, GivenMultiDispatchWithoutKernelWhenDevicePreemptionIsMidThreadThenTaskPreemptionIsMidThread) {
    dispatchInfo.reset(new DispatchInfo(device.get(), nullptr, 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0)));

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);

    EXPECT_EQ(PreemptionMode::MidThread, ClPreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}
