/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/api/additional_extensions.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

struct AdditionalKernelExecInfoFixture : public ApiFixture<> {
    void setUp() {
        ApiFixture::setUp();

        auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto revId = HwInfoConfig::get(hwInfo->platform.eProductFamily)->getHwRevIdFromStepping(REVISION_B, *hwInfo);
        hwInfo->platform.usRevId = revId;
    }
};

using AdditionalKernelExecInfoTests = Test<AdditionalKernelExecInfoFixture>;

using matcherDG2AndLater = IsAtLeastXeHpgCore;
HWTEST2_F(AdditionalKernelExecInfoTests, givenEuThreadOverDispatchEnableWhenCallingClSetKernelExecInfoThenOverDispatchIsDisabled, matcherDG2AndLater) {
    std::unique_ptr<KernelInfo> pKernelInfo = std::make_unique<KernelInfo>();
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MockMultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex)));
    auto pKernel = pMultiDeviceKernel->getKernel(testedRootDeviceIndex);
    const auto &hwInfo = pDevice->getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    cl_kernel_exec_info paramName = CL_KERNEL_EXEC_INFO_EU_THREAD_OVER_DISPATCH_INTEL;
    cl_bool paramValue = CL_FALSE;
    cl_int retVal = clSetKernelExecInfo(pMultiDeviceKernel.get(), paramName, sizeof(cl_bool), &paramValue);

    if (hwInfoConfig.isDisableOverdispatchAvailable(hwInfo)) {
        EXPECT_EQ(CL_SUCCESS, retVal);
    } else {
        EXPECT_EQ(CL_INVALID_VALUE, retVal);
    }
    EXPECT_EQ(AdditionalKernelExecInfo::DisableOverdispatch, pKernel->getAdditionalKernelExecInfo());
}

HWTEST2_F(AdditionalKernelExecInfoTests, givenNullPtrParamWhenSetKernelExecInfoSetThenReturnError, matcherDG2AndLater) {
    std::unique_ptr<KernelInfo> pKernelInfo = std::make_unique<KernelInfo>();
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MockMultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex)));
    auto pKernel = pMultiDeviceKernel->getKernel(testedRootDeviceIndex);

    cl_kernel_exec_info paramName = CL_KERNEL_EXEC_INFO_EU_THREAD_OVER_DISPATCH_INTEL;
    cl_int retVal = clSetKernelExecInfo(pMultiDeviceKernel.get(), paramName, sizeof(cl_bool), nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(AdditionalKernelExecInfo::DisableOverdispatch, pKernel->getAdditionalKernelExecInfo());
}

HWTEST2_F(AdditionalKernelExecInfoTests, givenIncorrectParamSizeWhenSetKernelExecInfoSetThenReturnError, matcherDG2AndLater) {
    std::unique_ptr<KernelInfo> pKernelInfo = std::make_unique<KernelInfo>();
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MockMultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex)));
    auto pKernel = pMultiDeviceKernel->getKernel(testedRootDeviceIndex);

    cl_kernel_exec_info paramName = CL_KERNEL_EXEC_INFO_EU_THREAD_OVER_DISPATCH_INTEL;
    cl_bool paramValue = CL_FALSE;
    size_t paramSize = sizeof(cl_bool) + 1;

    cl_int retVal = clSetKernelExecInfo(pMultiDeviceKernel.get(), paramName, paramSize, &paramValue);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(AdditionalKernelExecInfo::DisableOverdispatch, pKernel->getAdditionalKernelExecInfo());
}

struct EnqueueTaskWithAdditionalKernelExecInfo : public HelloWorldTest<HelloWorldFixtureFactory> {
    using Parent = HelloWorldTest<HelloWorldFixtureFactory>;

    void SetUp() override {
        Parent::kernelFilename = "required_work_group";
        Parent::kernelName = "CopyBuffer2";
        Parent::SetUp();
    }

    void TearDown() override {
        Parent::TearDown();
    }
};

HWTEST2_F(EnqueueTaskWithAdditionalKernelExecInfo, givenKernelWithOverDispatchDisabledWhenEnqueuingTaskThenOverdispatchIsDisabledSuccessIsReturned, matcherDG2AndLater) {
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *hwInfo);

    if (!hwInfoConfig.isDisableOverdispatchAvailable(*hwInfo)) {
        GTEST_SKIP();
    }

    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;
    cl_kernel_exec_info paramName = CL_KERNEL_EXEC_INFO_EU_THREAD_OVER_DISPATCH_INTEL;
    cl_bool paramValue = CL_FALSE;

    retVal = clSetKernelExecInfo(pMultiDeviceKernel, paramName, sizeof(cl_bool), &paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(AdditionalKernelExecInfo::DisableOverdispatch, pKernel->getAdditionalKernelExecInfo());

    cl_command_queue commandQueue = static_cast<cl_command_queue>(pCmdQ);
    cl_kernel kernel = static_cast<cl_kernel>(pMultiDeviceKernel);

    retVal = clEnqueueTask(
        commandQueue,
        kernel,
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST2_F(AdditionalKernelExecInfoTests, givenEuThreadOverDispatchEnableWhenCallingClSetKernelExecInfoThenThenStateIsTheSameAndSuccessIsReturned, IsAtLeastXeHpCore) {
    const auto &hwInfo = pDevice->getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    if (!hwInfoConfig.isDisableOverdispatchAvailable(hwInfo)) {
        GTEST_SKIP();
    }

    std::unique_ptr<KernelInfo> pKernelInfo = std::make_unique<KernelInfo>();
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MockMultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, testedRootDeviceIndex)));
    auto pKernel = pMultiDeviceKernel->getKernel(testedRootDeviceIndex);
    EXPECT_EQ(AdditionalKernelExecInfo::DisableOverdispatch, pKernel->getAdditionalKernelExecInfo());

    cl_kernel_exec_info paramName = CL_KERNEL_EXEC_INFO_EU_THREAD_OVER_DISPATCH_INTEL;
    cl_bool paramValue = CL_TRUE;
    cl_int retVal = clSetKernelExecInfo(pMultiDeviceKernel.get(), paramName, sizeof(cl_bool), &paramValue);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(AdditionalKernelExecInfo::NotSet, pKernel->getAdditionalKernelExecInfo());
}
