/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"

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

TEST_F(ThreadGroupPreemptionTests, disallowByKMD) {
    PreemptionFlags flags = {};
    waTable->waDisablePerCtxtPreemptionGranularityControl = 1;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, disallowByDevice) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidThread, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, disallowByReadWriteFencesWA) {
    PreemptionFlags flags = {};
    executionEnvironment->UsesFencesForReadWriteImages = 1u;
    waTable->waDisableLSQCROPERFforOCL = 1;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, disallowBySchedulerKernel) {
    PreemptionFlags flags = {};
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device, true));
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, disallowByVmeKernel) {
    PreemptionFlags flags = {};
    kernelInfo->isVmeWorkload = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, simpleAllow) {
    PreemptionFlags flags = {};
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, allowDefaultModeForNonKernelRequest) {
    PreemptionFlags flags = {};
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), nullptr);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithNoEnvironmentPatchSetWhenLSQCWaIsTurnedOnThenThreadGroupPreemptionIsBeingSelected) {
    PreemptionFlags flags = {};
    kernelInfo.get()->patchInfo.executionEnvironment = nullptr;
    waTable->waDisableLSQCROPERFforOCL = 1;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithEnvironmentPatchSetWhenLSQCWaIsTurnedOnThenThreadGroupPreemptionIsBeingSelected) {
    PreemptionFlags flags = {};
    executionEnvironment.get()->UsesFencesForReadWriteImages = 0;
    waTable->waDisableLSQCROPERFforOCL = 1;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, givenKernelWithEnvironmentPatchSetWhenLSQCWaIsTurnedOffThenThreadGroupPreemptionIsBeingSelected) {
    PreemptionFlags flags = {};
    executionEnvironment.get()->UsesFencesForReadWriteImages = 1;
    waTable->waDisableLSQCROPERFforOCL = 0;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, allowMidBatch) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidBatch);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), nullptr);
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, disallowWhenAdjustedDisabled) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::Disabled);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), nullptr);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(ThreadGroupPreemptionTests, returnDefaultDeviceModeForZeroSizedMdi) {
    MultiDispatchInfo multiDispatchInfo;
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, returnDefaultDeviceModeForValidKernelsInMdi) {
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, disallowDefaultDeviceModeForValidKernelsInMdiAndDisabledPremption) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, disallowDefaultDeviceModeWhenAtLeastOneInvalidKernelInMdi) {
    MockKernel schedulerKernel(program.get(), *kernelInfo, *device, true);
    DispatchInfo schedulerDispatchInfo(&schedulerKernel, 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0));

    PreemptionFlags flags = {};
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), &schedulerKernel);
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(schedulerDispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);

    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(device->getDevice(), multiDispatchInfo));
}

TEST_F(MidThreadPreemptionTests, allowMidThreadPreemption) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment->DisableMidThreadPreemption = 0;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, allowMidThreadPreemptionNullKernel) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), nullptr);
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, allowMidThreadPreemptionDeviceSupportPreemptionOnVmeKernel) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = true;
    kernelInfo->isVmeWorkload = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, disallowMidThreadPreemptionByDevice) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    executionEnvironment->DisableMidThreadPreemption = 0;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(flags));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags));
}

TEST_F(MidThreadPreemptionTests, disallowMidThreadPreemptionByKernel) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment->DisableMidThreadPreemption = 1;
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, disallowMidThreadPreemptionByVmeKernel) {
    PreemptionFlags flags = {};
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = false;
    kernelInfo->isVmeWorkload = true;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(flags));
}

TEST_F(MidThreadPreemptionTests, taskPreemptionDisallowMidThreadByDevice) {
    PreemptionFlags flags = {};
    executionEnvironment->DisableMidThreadPreemption = 0;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, taskPreemptionDisallowMidThreadByKernel) {
    PreemptionFlags flags = {};
    executionEnvironment->DisableMidThreadPreemption = 1;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, taskPreemptionDisallowMidThreadByVmeKernel) {
    PreemptionFlags flags = {};
    kernelInfo->isVmeWorkload = true;
    device->sharedDeviceInfo.vmeAvcSupportsPreemption = false;
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    //VME disables mid thread and thread group when device does not support it
    EXPECT_EQ(PreemptionMode::MidBatch, outMode);
}

TEST_F(MidThreadPreemptionTests, taskPreemptionAllow) {
    PreemptionFlags flags = {};
    executionEnvironment->DisableMidThreadPreemption = 0;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionHelper::setPreemptionLevelFlags(flags, device->getDevice(), kernel.get());
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(device->getPreemptionMode(), flags);
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(MidThreadPreemptionTests, taskPreemptionAllowDeviceSupportsPreemptionOnVmeKernel) {
    PreemptionFlags flags = {};
    executionEnvironment->DisableMidThreadPreemption = 0;
    kernelInfo->isVmeWorkload = true;
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
