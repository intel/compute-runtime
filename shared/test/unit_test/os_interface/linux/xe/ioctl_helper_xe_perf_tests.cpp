/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/os_interface/linux/xe/mock_drm_xe.h"
#include "shared/test/common/os_interface/linux/xe/mock_drm_xe_perf.h"
#include "shared/test/common/os_interface/linux/xe/mock_ioctl_helper_xe.h"
#include "shared/test/common/os_interface/linux/xe/xe_config_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
using IoctlHelperXeTest = Test<XeConfigFixture>;

TEST_F(IoctlHelperXeTest, whenCallingGetIoctlRequestValuePerfThenCorrectValueisReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_EQ(DRM_IOCTL_XE_OBSERVATION, xeIoctlHelper->getIoctlRequestValuePerf(DrmIoctl::perfOpen));
    EXPECT_EQ(DRM_XE_OBSERVATION_IOCTL_ENABLE, xeIoctlHelper->getIoctlRequestValuePerf(DrmIoctl::perfEnable));
    EXPECT_EQ(DRM_XE_OBSERVATION_IOCTL_DISABLE, xeIoctlHelper->getIoctlRequestValuePerf(DrmIoctl::perfDisable));
    EXPECT_EQ(DRM_IOCTL_XE_DEVICE_QUERY, xeIoctlHelper->getIoctlRequestValuePerf(DrmIoctl::perfQuery));
    EXPECT_EQ(0u, xeIoctlHelper->getIoctlRequestValuePerf(DrmIoctl::version));
}

TEST_F(IoctlHelperXeTest, givenXeHelperWhenCallingPerfOpenEuStallStreamWithInvalidArgumentsThenFailureReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t invalidFd = -1;
    xeIoctlHelper->failPerfOpen = true;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_FALSE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &invalidFd));
}

TEST_F(IoctlHelperXeTest, givenCalltoPerfOpenEuStallStreamWithInvalidStreamWithEnableSetToFailThenFailureReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t invalidFd = -1;
    xeIoctlHelper->failPerfEnable = true;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_FALSE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &invalidFd));
}

TEST_F(IoctlHelperXeTest, givenCalltoPerfDisableEuStallStreamWithInvalidStreamThenFailureIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t invalidFd = -1;
    xeIoctlHelper->failPerfDisable = true;
    EXPECT_FALSE(xeIoctlHelper->perfDisableEuStallStream(&invalidFd));
}

TEST_F(IoctlHelperXeTest, givenCalltoPerfOpenEuStallStreamWithValidStreamAndPerfEnableFailsThenFailureIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t validFd = -1;
    uint32_t samplingPeriodNs = 10000u;
    xeIoctlHelper->failPerfEnable = true;
    EXPECT_FALSE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeriodNs, 1, 20u, 10000u, &validFd));
}

TEST_F(IoctlHelperXeTest, whenCallingPerfOpenEuStallStreamThenCloseThenTrueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t validFd = -1;
    uint32_t samplingPeriodNs = 50000000u;
    EXPECT_TRUE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeriodNs, 1, 20u, 10000u, &validFd));
    EXPECT_TRUE(xeIoctlHelper->perfDisableEuStallStream(&validFd));
}

TEST_F(IoctlHelperXeTest, whenCallingPerfOpenEuStallStreamThenCloseWithSmallestSamplingRateValueThenTrueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t validFd = -1;
    uint32_t samplingPeriodNs = 10u;
    EXPECT_TRUE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeriodNs, 1, 20u, 10000u, &validFd));
    EXPECT_TRUE(xeIoctlHelper->perfDisableEuStallStream(&validFd));
}

TEST_F(IoctlHelperXeTest, whenCallingPerfOpenEuStallStreamThenCloseWithLargestSamplingRateValueThenTrueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t validFd = -1;
    uint32_t samplingPeriodNs = 1000000000u;
    EXPECT_TRUE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeriodNs, 1, 20u, 10000u, &validFd));
    EXPECT_TRUE(xeIoctlHelper->perfDisableEuStallStream(&validFd));
}

TEST_F(IoctlHelperXeTest, whenCallingPerfOpenEuStallStreamThenCloseWithDifferentSampligRatesToCoverIfBranchThenTrueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t validFd = -1;
    uint32_t samplingPeriodNs = 25200000u;
    EXPECT_TRUE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeriodNs, 1, 20u, 10000u, &validFd));
    EXPECT_TRUE(xeIoctlHelper->perfDisableEuStallStream(&validFd));
}

TEST_F(IoctlHelperXeTest, givenXeHelperWhenCallingPerfOpenEuStallStreamAndQuerySizeIsZeroThenFailureReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t validFd = -1;
    uint32_t samplingPeridNs = 10000u;
    drm->querySizeZero = true;
    EXPECT_FALSE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &validFd));
}

TEST_F(IoctlHelperXeTest, givenXeHelperWhenCallingPerfOpenEuStallStreamAndNumSamplingRatesIsZeroThenFailureReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t validFd = -1;
    uint32_t samplingPeridNs = 10000u;
    drm->numSamplingRateCountZero = true;
    EXPECT_FALSE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &validFd));
}

TEST_F(IoctlHelperXeTest, givenXeHelperWhenCallingPerfOpenEuStallStreamAndIoctlFailsThenFailureReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t validFd = -1;
    uint32_t samplingPeridNs = 10000u;
    drm->failPerfQueryOnCall = 2;
    EXPECT_FALSE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &validFd));
}

TEST_F(IoctlHelperXeTest, givenXeHelperWhenCallingPerfOpenEuStallStreamAndQueryIoctlFailsThenFailureReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t validFd = -1;
    uint32_t samplingPeridNs = 10000u;
    xeIoctlHelper->failPerfQuery = true;
    EXPECT_FALSE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &validFd));
}

TEST_F(IoctlHelperXeTest, givenXeHelperWhenCallingPerfOpenEuStallStreamAndOpenIoctlFailsThenFailureReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    int32_t validFd = -1;
    uint32_t samplingPeridNs = 10000u;
    xeIoctlHelper->failPerfOpen = true;
    EXPECT_FALSE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &validFd));
}

TEST_F(IoctlHelperXeTest, givenCalltoPerfDisableEuStallStreamWithValidStreamButCloseFailsThenFailureReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fileDescriptor) -> int {
        return -1;
    });

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncRetVal = -1;
    int32_t invalidFd = 10;
    EXPECT_FALSE(xeIoctlHelper->perfDisableEuStallStream(&invalidFd));
    EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);
    EXPECT_EQ(10, NEO::SysCalls::closeFuncArgPassed);
    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;
    NEO::SysCalls::closeFuncRetVal = 0;
}

TEST_F(IoctlHelperXeTest, givenCalltoPerfOpenEuStallStreamWithValidStreamButFcntlFailsThenFailureReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePerf::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    xeIoctlHelper->initialize();
    SysCalls::failFcntl = true;
    int32_t validFd = -1;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_FALSE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &validFd));

    SysCalls::failFcntl = false;
    SysCalls::failFcntl1 = true;
    EXPECT_FALSE(xeIoctlHelper->perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &validFd));
    SysCalls::failFcntl1 = false;
}

TEST_F(IoctlHelperXeTest, whenCallingIsEuStallSupportedThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    EXPECT_TRUE(xeIoctlHelper.get()->isEuStallSupported());
}

TEST_F(IoctlHelperXeTest, whenCallingPerfOpenEuStallStreamWithNoIoctlSupportThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    int32_t invalidFd = -1;
    uint32_t samplingPeriodNs = 10000u;
    EXPECT_FALSE(xeIoctlHelper.get()->perfOpenEuStallStream(0u, samplingPeriodNs, 1, 20u, 10000u, &invalidFd));
}

TEST_F(IoctlHelperXeTest, whenCallingPerfDisableEuStallStreamWithNoIoctlSupportThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
    int32_t invalidFd = -1;
    EXPECT_FALSE(xeIoctlHelper.get()->perfDisableEuStallStream(&invalidFd));
}