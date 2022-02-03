/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"

namespace NEO {
namespace SysCalls {
extern int closeFuncRetVal;

} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace ult {

class IoctlHelperPrelim20Mock : public NEO::IoctlHelperPrelim20 {

    bool getEuStallProperties(std::array<uint64_t, 10u> &properties, uint64_t dssBufferSize, uint64_t samplingRate, uint64_t pollPeriod, uint64_t engineInstance) override {
        return false;
    }
};

class DrmPrelimMock : public DrmMock {
  public:
    DrmPrelimMock(RootDeviceEnvironment &rootDeviceEnvironment) : DrmPrelimMock(rootDeviceEnvironment, defaultHwInfo.get()) {}
    DrmPrelimMock(RootDeviceEnvironment &rootDeviceEnvironment, HardwareInfo *inputHwInfo) : DrmMock(rootDeviceEnvironment) {
        inputHwInfo->gtSystemInfo.MaxDualSubSlicesSupported = 64;
        rootDeviceEnvironment.setHwInfo(inputHwInfo);
        setupIoctlHelper(rootDeviceEnvironment.getHardwareInfo()->platform.eProductFamily);
        recentProperties.fill(255);
    }

    void getPrelimVersion(std::string &prelimVersion) override {
        prelimVersion = "2.0";
    }

    int handleRemainingRequests(unsigned long request, void *arg) override {

        if (request == DRM_IOCTL_I915_PERF_OPEN) {
            drm_i915_perf_open_param *param = reinterpret_cast<drm_i915_perf_open_param *>(arg);
            std::memcpy(recentProperties.data(), reinterpret_cast<void *>(param->properties_ptr), param->num_properties * 8 * 2);
            return ioctli915PerfOpenReturn;
        }
        return -1;
    }

    void setIoctlHelperPrelim20Mock() {
        backUpIoctlHelper = std::move(ioctlHelper);
        ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<IoctlHelperPrelim20Mock>());
    }

    void restoreIoctlHelperPrelim20() {
        ioctlHelper = std::move(backUpIoctlHelper);
    }

    std::unique_ptr<NEO::IoctlHelper> backUpIoctlHelper;
    int ioctli915PerfOpenReturn = 1;
    std::array<uint64_t, 10u> recentProperties{};
};

class MetricIpSamplingLinuxTestPrelim : public DeviceFixture,
                                        public ::testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = device->getOsInterface();
        osInterface.setDriverModel(std::make_unique<DrmPrelimMock>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));

        metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*device));
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }
    std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface = nullptr;
};

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenCorrectArgumentsWhenStartMeasurementIsCalledThenReturnSuccess, IsPVC) {

    constexpr uint32_t samplingGranularity = 251;
    constexpr uint32_t gpuClockPeriodNs = 1000000;
    constexpr uint32_t samplingUnit = 3;
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = samplingGranularity * samplingUnit * gpuClockPeriodNs;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
    EXPECT_EQ(samplingPeriodNs, samplingGranularity * samplingUnit * gpuClockPeriodNs);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenGetTimestampFrequencyFailsWhenStartMeasurementIsCalledThenReturnFailure, IsPVC) {

    auto drm = static_cast<DrmPrelimMock *>(device->getOsInterface().getDriverModel()->as<NEO::Drm>());
    VariableBackup<int> backupCsTimeStampFrequency(&drm->storedCsTimestampFrequency, 0);
    VariableBackup<int> backupStoredRetVal(&drm->storedRetVal, -1);

    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenIoctlI915PerfOpenFailsWhenStartMeasurementIsCalledThenReturnFailure, IsPVC) {

    auto drm = static_cast<DrmPrelimMock *>(device->getOsInterface().getDriverModel()->as<NEO::Drm>());
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    VariableBackup<int> backupI915PerfOpenReturn(&drm->ioctli915PerfOpenReturn, -1);
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenI915PerfIoctlEnableFailsWhenStartMeasurementIsCalledThenReturnFailure, IsPVC) {

    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl, [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == I915_PERF_IOCTL_ENABLE) {
            return -1;
        }
        return 0;
    });

    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenCloseSucceedsWhenStopMeasurementIsCalledThenReturnSuccess, IsPVC) {

    VariableBackup<int> backupCloseFuncRetval(&NEO::SysCalls::closeFuncRetVal, 0);
    EXPECT_EQ(metricIpSamplingOsInterface->stopMeasurement(), ZE_RESULT_SUCCESS);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenCloseFailsWhenStopMeasurementIsCalledThenReturnFailure, IsPVC) {

    VariableBackup<int> backupCloseFuncRetval(&NEO::SysCalls::closeFuncRetVal, -1);
    EXPECT_EQ(metricIpSamplingOsInterface->stopMeasurement(), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenPreadSucceedsWhenReadDataIsCalledThenReturnSuccess, IsPVC) {

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return 1;
    });
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_SUCCESS);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenPreadFailsWhenReadDataIsCalledThenReturnFailure, IsPVC) {

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return -1;
    });
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, WhenGetRequiredBufferSizeIsCalledThenCorrectSizeIsReturned, IsPVC) {

    constexpr uint32_t unitReportSize = 64;
    EXPECT_EQ(metricIpSamplingOsInterface->getRequiredBufferSize(10), unitReportSize * 10);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, GivenPollIsSuccessfulWhenisNReportsAvailableIsCalledThenReturnSuccess, IsPVC) {

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    EXPECT_TRUE(metricIpSamplingOsInterface->isNReportsAvailable());
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, GivenPollIsFailureWhenisNReportsAvailableIsCalledThenReturnFailure, IsPVC) {

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return -1;
    });
    EXPECT_FALSE(metricIpSamplingOsInterface->isNReportsAvailable());
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, GivenUnsupportedProductFamilyIsUsedWhenIsDependencyAvailableIsCalledThenReturnFailure, IsDG2) {

    auto hwInfo = neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = productFamily;
    EXPECT_FALSE(metricIpSamplingOsInterface->isDependencyAvailable());
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, GivenSupportedProductFamilyAndUnsupportedDeviceIdIsUsedWhenIsDependencyAvailableIsCalledThenReturnFailure, IsPVC) {

    auto hwInfo = neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = productFamily;
    hwInfo->platform.usDeviceID = NEO::XE_HPC_CORE::pvcXlDeviceId;
    EXPECT_FALSE(metricIpSamplingOsInterface->isDependencyAvailable());
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, GivenSupportedProductFamilyAndSupportedDeviceIdIsUsedWhenIsDependencyAvailableIsCalledThenReturnFailure, IsPVC) {

    auto hwInfo = neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = productFamily;

    const std::vector<uint32_t> supportedDeviceIds = {
        NEO::XE_HPC_CORE::pvcXtDeviceIds[0],
        NEO::XE_HPC_CORE::pvcXtDeviceIds[1],
        NEO::XE_HPC_CORE::pvcXtDeviceIds[2],
        NEO::XE_HPC_CORE::pvcXtDeviceIds[3],
        NEO::XE_HPC_CORE::pvcXtDeviceIds[4]};

    for (uint32_t deviceId : supportedDeviceIds) {
        hwInfo->platform.usDeviceID = deviceId;
        EXPECT_TRUE(metricIpSamplingOsInterface->isDependencyAvailable());
    }
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, GivenDriverOpenFailsWhenIsDependencyAvailableIsCalledThenReturnFailure, IsPVC) {

    auto hwInfo = neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = productFamily;
    hwInfo->platform.usDeviceID = NEO::XE_HPC_CORE::pvcXtDeviceIds[0];

    auto drm = static_cast<DrmPrelimMock *>(device->getOsInterface().getDriverModel()->as<NEO::Drm>());
    VariableBackup<int> backupCsTimeStampFrequency(&drm->storedCsTimestampFrequency, 0);
    VariableBackup<int> backupStoredRetVal(&drm->storedRetVal, -1);

    EXPECT_FALSE(metricIpSamplingOsInterface->isDependencyAvailable());
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, GivenIoctlHelperFailsWhenIsDependencyAvailableIsCalledThenReturnFailure, IsPVC) {

    auto hwInfo = neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = productFamily;
    hwInfo->platform.usDeviceID = NEO::XE_HPC_CORE::pvcXtDeviceIds[0];

    auto drm = static_cast<DrmPrelimMock *>(device->getOsInterface().getDriverModel()->as<NEO::Drm>());

    drm->setIoctlHelperPrelim20Mock();
    EXPECT_FALSE(metricIpSamplingOsInterface->isDependencyAvailable());
    drm->restoreIoctlHelperPrelim20();
}

struct MetricIpSamplingLinuxMultiDeviceTest : public ::testing::Test {

    std::unique_ptr<UltDeviceFactory> createDevices(uint32_t numSubDevices) {
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        NEO::ExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, 1);
        executionEnvironment->parseAffinityMask();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(
            std::make_unique<DrmPrelimMock>(const_cast<NEO::RootDeviceEnvironment &>(*executionEnvironment->rootDeviceEnvironments[0])));
        return std::make_unique<UltDeviceFactory>(1, numSubDevices, *executionEnvironment);
    }

    DebugManagerStateRestore restorer;
};

HWTEST2_F(MetricIpSamplingLinuxMultiDeviceTest, GivenCombinationOfAffinityMaskWhenStartMeasurementIsCalledForRootDeviceThenInstanceIdIsCorrect, IsPVC) {
    DebugManager.flags.ZE_AFFINITY_MASK.set("0.1,0.2,0.3");

    auto deviceFactory = createDevices(4);
    auto driverHandle = std::make_unique<DriverHandleImp>();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto rootDevice = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), deviceFactory->rootDevices[0], false, &returnValue));
    auto metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*rootDevice));
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
    // verifying engine instance is set correctly
    auto drm = static_cast<DrmPrelimMock *>(rootDevice->getOsInterface().getDriverModel()->as<NEO::Drm>());
    EXPECT_EQ(drm->recentProperties[9], 1ull);
}

HWTEST2_F(MetricIpSamplingLinuxMultiDeviceTest, GivenCombinationOfAffinityMaskWhenStartMeasurementIsCalledForSubDeviceThenInstanceIdIsCorrect, IsPVC) {
    DebugManager.flags.ZE_AFFINITY_MASK.set("0.2,0.3");

    auto deviceFactory = createDevices(4);
    auto driverHandle = std::make_unique<DriverHandleImp>();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto rootDevice = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), deviceFactory->rootDevices[0], false, &returnValue));

    uint32_t subDeviceCount = 2;
    ze_device_handle_t subDevices[2] = {};

    rootDevice->getSubDevices(&subDeviceCount, subDevices);
    auto metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*subDevices[0]));
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
    // verifying engine instance is set correctly
    auto drm = static_cast<DrmPrelimMock *>(rootDevice->getOsInterface().getDriverModel()->as<NEO::Drm>());
    EXPECT_EQ(drm->recentProperties[9], 2ull);

    metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*subDevices[1]));
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
    EXPECT_EQ(drm->recentProperties[9], 3ull);
}

} // namespace ult
} // namespace L0
