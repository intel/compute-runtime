/*
 * Copyright (c) 2017, Intel Corporation
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

#include "unit_tests/preemption/preemption_tests.h"

using namespace OCLRT;

typedef DevicePreemptionTests ThreadGroupPreemptionTests;
typedef DevicePreemptionTests MidThreadPreemptionTests;

TEST_F(ThreadGroupPreemptionTests, disallowByKMD) {
    waTable->waDisablePerCtxtPreemptionGranularityControl = 1;

    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(kernel, waTable));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, kernel));
}

TEST_F(ThreadGroupPreemptionTests, disallowByDevice) {
    device->setPreemptionMode(PreemptionMode::MidThread);

    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel, waTable));
    EXPECT_EQ(PreemptionMode::MidThread, PreemptionHelper::taskPreemptionMode(*device, kernel));
}

TEST_F(ThreadGroupPreemptionTests, disallowByReadWriteFencesWA) {
    executionEnvironment.UsesFencesForReadWriteImages = 1u;
    waTable->waDisableLSQCROPERFforOCL = 1;

    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(kernel, waTable));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, kernel));
}

TEST_F(ThreadGroupPreemptionTests, disallowBySchedulerKernel) {
    delete kernel;
    kernel = new MockKernel(&program, *kernelInfo, *device, true);

    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(kernel, waTable));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, kernel));
}

TEST_F(ThreadGroupPreemptionTests, disallowByVmeKernel) {
    delete kernel;
    kernelInfo->isVmeWorkload = true;
    kernel = new MockKernel(&program, *kernelInfo, *device);

    EXPECT_FALSE(PreemptionHelper::allowThreadGroupPreemption(kernel, waTable));
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, kernel));
}

TEST_F(ThreadGroupPreemptionTests, simpleAllow) {
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel, waTable));
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, kernel));
}

TEST_F(ThreadGroupPreemptionTests, allowDefaultModeForNonKernelRequest) {
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, nullptr));
}

TEST_F(ThreadGroupPreemptionTests, allowMidBatch) {
    device->setPreemptionMode(PreemptionMode::MidBatch);
    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, nullptr));
}

TEST_F(ThreadGroupPreemptionTests, disallowWhenAdjustedDisabled) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(*device, nullptr));
}

TEST_F(ThreadGroupPreemptionTests, returnDefaultDeviceModeForZeroSizedMdi) {
    MultiDispatchInfo multiDispatchInfo;
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, returnDefaultDeviceModeForValidKernelsInMdi) {
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, disallowDefaultDeviceModeForValidKernelsInMdiAndDisabledPremption) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo));
}

TEST_F(ThreadGroupPreemptionTests, disallowDefaultDeviceModeWhenAtLeastOneInvalidKernelInMdi) {
    MockKernel schedulerKernel(&program, *kernelInfo, *device, true);
    DispatchInfo schedulerDispatchInfo(&schedulerKernel, 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0));

    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, &schedulerKernel));

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(*dispatchInfo);
    multiDispatchInfo.push(schedulerDispatchInfo);
    multiDispatchInfo.push(*dispatchInfo);

    EXPECT_EQ(PreemptionMode::MidBatch, PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo));
}

TEST_F(MidThreadPreemptionTests, allowMidThreadPreemption) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment.DisableMidThreadPreemption = 0;
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(kernel, *device));
}

TEST_F(MidThreadPreemptionTests, allowMidThreadPreemptionNullKernel) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(nullptr, *device));
}

TEST_F(MidThreadPreemptionTests, disallowMidThreadPreemptionByDevice) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    executionEnvironment.DisableMidThreadPreemption = 0;
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(kernel, *device));
}

TEST_F(MidThreadPreemptionTests, disallowMidThreadPreemptionByKernel) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment.DisableMidThreadPreemption = 1;
    EXPECT_FALSE(PreemptionHelper::allowMidThreadPreemption(kernel, *device));
}

TEST_F(MidThreadPreemptionTests, taskPreemptionDisallowMidThreadByDevice) {
    executionEnvironment.DisableMidThreadPreemption = 0;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(*device, kernel);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, taskPreemptionDisallowMidThreadByKernel) {
    executionEnvironment.DisableMidThreadPreemption = 1;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(*device, kernel);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outMode);
}

TEST_F(MidThreadPreemptionTests, taskPreemptionAllow) {
    executionEnvironment.DisableMidThreadPreemption = 0;
    device->setPreemptionMode(PreemptionMode::MidThread);
    PreemptionMode outMode = PreemptionHelper::taskPreemptionMode(*device, kernel);
    EXPECT_EQ(PreemptionMode::MidThread, outMode);
}

TEST_F(DevicePreemptionTests, setDefaultMidThreadPreemption) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::MidThread, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultThreadGroupPreemptionNoMidThreadDefault) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::ThreadGroup;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::ThreadGroup, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultThreadGroupPreemptionNoMidThreadSupport) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, false, true, true);
    EXPECT_EQ(PreemptionMode::ThreadGroup, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultMidBatchPreemptionNoThreadGroupDefault) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidBatch;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::MidBatch, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultMidBatchPreemptionNoThreadGroupSupport) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, false, false, true);
    EXPECT_EQ(PreemptionMode::MidBatch, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultDisabledPreemptionNoMidBatchDefault) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::Disabled;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, true, true, true);
    EXPECT_EQ(PreemptionMode::Disabled, devCapabilities.defaultPreemptionMode);
}

TEST_F(DevicePreemptionTests, setDefaultDisabledPreemptionNoMidBatchSupport) {
    RuntimeCapabilityTable devCapabilities = {};

    devCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;

    PreemptionHelper::adjustDefaultPreemptionMode(devCapabilities, false, false, false);
    EXPECT_EQ(PreemptionMode::Disabled, devCapabilities.defaultPreemptionMode);
}

TEST(PreemptionTest, defaultMode) {
    EXPECT_EQ(0, DebugManager.flags.ForcePreemptionMode.get());
}
