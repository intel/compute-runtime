/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

namespace NEO {
namespace SysCalls {
extern int closeFuncRetVal;

} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace ult {

class IoctlHelperPrelim20Mock : public NEO::IoctlHelperPrelim20 {
  public:
    using NEO::IoctlHelperPrelim20::IoctlHelperPrelim20;
    bool perfOpenEuStallStream(uint32_t euStallFdParameter, uint32_t &samplingPeriodNs, uint64_t engineInstance, uint64_t notifyNReports, uint64_t gpuTimeStampfrequency, int32_t *stream) override {
        return false;
    }
};

class DrmPrelimMock : public DrmMock {
  public:
    DrmPrelimMock(RootDeviceEnvironment &rootDeviceEnvironment) : DrmPrelimMock(rootDeviceEnvironment, defaultHwInfo.get()) {}
    DrmPrelimMock(RootDeviceEnvironment &rootDeviceEnvironment, HardwareInfo *inputHwInfo, bool invokeQueryEngineInfo = true) : DrmMock(rootDeviceEnvironment) {
        customHwInfo = std::make_unique<NEO::HardwareInfo>(&inputHwInfo->platform, &inputHwInfo->featureTable,
                                                           &inputHwInfo->workaroundTable, &inputHwInfo->gtSystemInfo, inputHwInfo->capabilityTable);
        customHwInfo->gtSystemInfo.MaxDualSubSlicesSupported = 64;
        rootDeviceEnvironment.setHwInfoAndInitHelpers(customHwInfo.get());
        this->ioctlHelper = std::make_unique<IoctlHelperPrelim20>(*this);
        if (invokeQueryEngineInfo) {
            queryEngineInfo(); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
        }
    }

    bool queryEngineInfo() override {
        uint16_t computeEngineClass = getIoctlHelper()->getDrmParamValue(DrmParam::engineClassCompute);
        std::vector<EngineCapabilities> engines(4);
        engines[0].engine = {computeEngineClass, 0};
        engines[0].capabilities = {};
        engines[1].engine = {computeEngineClass, 1};
        engines[1].capabilities = {};
        engines[2].engine = {computeEngineClass, 2};
        engines[2].capabilities = {};
        engines[3].engine = {computeEngineClass, 3};
        engines[3].capabilities = {};

        std::vector<DistanceInfo> distances(4);
        distances[0].engine = engines[0].engine;
        distances[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
        distances[1].engine = engines[1].engine;
        distances[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
        distances[2].engine = engines[2].engine;
        distances[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2};
        distances[3].engine = engines[3].engine;
        distances[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 3};

        std::vector<QueryItem> queryItems{distances.size()};
        for (auto i = 0u; i < distances.size(); i++) {
            queryItems[i].length = sizeof(drm_i915_query_engine_info);
        }

        engineInfo = std::make_unique<EngineInfo>(this, 4, distances, queryItems, engines);
        return true;
    }

    bool queryEngineInfo1SubDevice() {
        uint16_t computeEngineClass = getIoctlHelper()->getDrmParamValue(DrmParam::engineClassCompute);
        std::vector<EngineCapabilities> engines(1);
        engines[0].engine = {computeEngineClass, 0};
        engines[0].capabilities = {};

        std::vector<DistanceInfo> distances(1);
        distances[0].engine = engines[0].engine;
        distances[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

        std::vector<QueryItem> queryItems{distances.size()};
        for (auto i = 0u; i < distances.size(); i++) {
            queryItems[i].length = sizeof(drm_i915_query_engine_info);
        }

        engineInfo = std::make_unique<EngineInfo>(this, 1, distances, queryItems, engines);
        return true;
    }

    void setIoctlHelperPrelim20Mock() {
        backUpIoctlHelper = std::move(ioctlHelper);
        ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<IoctlHelperPrelim20Mock>(*this));
    }

    void restoreIoctlHelperPrelim20() {
        ioctlHelper = std::move(backUpIoctlHelper);
    }

    std::unique_ptr<NEO::HardwareInfo> customHwInfo;
    std::unique_ptr<NEO::IoctlHelper> backUpIoctlHelper;
};

class MetricIpSamplingLinuxTestPrelim : public DeviceFixture,
                                        public ::testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = *device->getOsInterface();
        osInterface.setDriverModel(std::make_unique<DrmPrelimMock>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));

        metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*device));
    }

    void TearDown() override {
        DeviceFixture::tearDown();
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

    auto drm = static_cast<DrmPrelimMock *>(device->getOsInterface()->getDriverModel()->as<NEO::Drm>());
    VariableBackup<int> backupCsTimeStampFrequency(&drm->storedCsTimestampFrequency, 0);
    VariableBackup<int> backupStoredRetVal(&drm->storedRetVal, -1);

    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenGetTimestampFrequencyReturnsFrequencyEqualZeroWhenStartMeasurementIsCalledThenReturnFailure, IsPVC) {

    auto drm = static_cast<DrmPrelimMock *>(device->getOsInterface()->getDriverModel()->as<NEO::Drm>());
    VariableBackup<int> backupCsTimeStampFrequency(&drm->storedCsTimestampFrequency, 0);

    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenIoctlI915PerfOpenFailsWhenStartMeasurementIsCalledThenReturnFailure, IsPVC) {

    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl, [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == DRM_IOCTL_I915_PERF_OPEN) {
            return -1;
        }
        return 0;
    });
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

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenPerfOpenEuStallStreamWhenStartMeasurementIsCalledThenReturnFailure, IsPVC) {

    auto drm = static_cast<DrmPrelimMock *>(device->getOsInterface()->getDriverModel()->as<NEO::Drm>());
    drm->setIoctlHelperPrelim20Mock();
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_ERROR_UNKNOWN);
    drm->restoreIoctlHelperPrelim20();
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenCloseSucceedsWhenStopMeasurementIsCalledThenReturnSuccess, IsPVC) {

    VariableBackup<int> backupCloseFuncRetval(&NEO::SysCalls::closeFuncRetVal, 0);
    EXPECT_EQ(metricIpSamplingOsInterface->stopMeasurement(), ZE_RESULT_SUCCESS);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenCloseFailsWhenStopMeasurementIsCalledThenReturnFailure, IsPVC) {

    VariableBackup<int> backupCloseFuncRetval(&NEO::SysCalls::closeFuncRetVal, -1);
    EXPECT_EQ(metricIpSamplingOsInterface->stopMeasurement(), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenI915PerfIoctlDisableFailsWhenStopMeasurementIsCalledThenReturnFailure, IsPVC) {

    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl, [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == I915_PERF_IOCTL_DISABLE) {
            return -1;
        }
        return 0;
    });

    EXPECT_EQ(metricIpSamplingOsInterface->stopMeasurement(), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenReadSucceedsWhenReadDataIsCalledThenReturnSuccess, IsPVC) {

    VariableBackup<decltype(SysCalls::sysCallsRead)> mockRead(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        return 1;
    });
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_SUCCESS);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenReadFailsWhenReadDataIsCalledThenReturnFailure, IsPVC) {

    VariableBackup<decltype(SysCalls::sysCallsRead)> mockRead(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        return -1;
        errno = EBADF;
    });
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, givenReadFailsWithRetryErrorNumberWhenReadDataIsCalledThenReturnSuccess, IsPVC) {

    VariableBackup<decltype(SysCalls::sysCallsRead)> mockRead(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        return -1;
    });
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    errno = EINTR;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_SUCCESS);
    errno = EBUSY;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_SUCCESS);
    errno = EAGAIN;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_SUCCESS);
    errno = EIO;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_WARNING_DROPPED_DATA);
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

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, GivenSupportedProductFamilyAndUnsupportedDeviceIdIsUsedWhenIsDependencyAvailableIsCalledThenReturnFailure, IsPVC) {

    auto hwInfo = neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = productFamily;
    hwInfo->platform.usDeviceID = NEO::pvcXlDeviceIds.front();
    EXPECT_FALSE(metricIpSamplingOsInterface->isDependencyAvailable());
}

HWTEST2_F(MetricIpSamplingLinuxTestPrelim, GivenSupportedProductFamilyAndSupportedDeviceIdIsUsedWhenIsDependencyAvailableIsCalledThenReturnSucess, IsPVC) {

    auto hwInfo = neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = productFamily;

    for (const auto &deviceId : NEO::pvcXtDeviceIds) {
        hwInfo->platform.usDeviceID = deviceId;
        EXPECT_TRUE(metricIpSamplingOsInterface->isDependencyAvailable());
    }
}

struct MetricIpSamplingLinuxMultiDeviceTest : public ::testing::Test {

    std::unique_ptr<UltDeviceFactory> createDevices(uint32_t numSubDevices) {
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        debugManager.flags.UseDrmVirtualEnginesForCcs.set(0);
        NEO::ExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, 1);
        executionEnvironment->parseAffinityMask();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(
            std::make_unique<DrmPrelimMock>(const_cast<NEO::RootDeviceEnvironment &>(*executionEnvironment->rootDeviceEnvironments[0]),
                                            defaultHwInfo.get(),
                                            false));
        return std::make_unique<UltDeviceFactory>(1, numSubDevices, *executionEnvironment);
    }

    DebugManagerStateRestore restorer;
};

HWTEST2_F(MetricIpSamplingLinuxMultiDeviceTest, GivenCombinationOfAffinityMaskWhenStartMeasurementIsCalledForRootDeviceThenInstanceIdIsCorrect, IsPVC) {
    debugManager.flags.ZE_AFFINITY_MASK.set("0.1,0.2,0.3");

    auto deviceFactory = createDevices(4);
    auto driverHandle = std::make_unique<DriverHandleImp>();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto rootDevice = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), deviceFactory->rootDevices[0], false, &returnValue));
    auto metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*rootDevice));
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    auto drm = static_cast<DrmPrelimMock *>(rootDevice->getOsInterface()->getDriverModel()->as<NEO::Drm>());
    drm->queryEngineInfo();
    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl, [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == DRM_IOCTL_I915_PERF_OPEN) {
            drm_i915_perf_open_param *param = reinterpret_cast<drm_i915_perf_open_param *>(arg);
            uint64_t *values = reinterpret_cast<uint64_t *>(param->properties_ptr);
            EXPECT_EQ(values[9], 1ull);
        }
        return 0;
    });
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
}

HWTEST2_F(MetricIpSamplingLinuxMultiDeviceTest, GivenCombinationOfAffinityMaskWhenStartMeasurementIsCalledForSubDeviceThenInstanceIdIsCorrect, IsPVC) {
    debugManager.flags.ZE_AFFINITY_MASK.set("0.2,0.3");

    auto deviceFactory = createDevices(4);
    auto driverHandle = std::make_unique<DriverHandleImp>();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto rootDevice = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), deviceFactory->rootDevices[0], false, &returnValue));

    uint32_t subDeviceCount = 2;
    ze_device_handle_t subDevices[2] = {};

    rootDevice->getSubDevices(&subDeviceCount, subDevices);
    auto metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*subDevices[0]));
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    auto drm = static_cast<DrmPrelimMock *>(rootDevice->getOsInterface()->getDriverModel()->as<NEO::Drm>());
    drm->queryEngineInfo();

    {
        VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl, [](int fileDescriptor, unsigned long int request, void *arg) -> int {
            if (request == DRM_IOCTL_I915_PERF_OPEN) {
                drm_i915_perf_open_param *param = reinterpret_cast<drm_i915_perf_open_param *>(arg);
                uint64_t *values = reinterpret_cast<uint64_t *>(param->properties_ptr);
                EXPECT_EQ(values[9], 2ull);
            }
            return 0;
        });

        EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
    }

    {
        metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*subDevices[1]));
        VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl, [](int fileDescriptor, unsigned long int request, void *arg) -> int {
            if (request == DRM_IOCTL_I915_PERF_OPEN) {
                drm_i915_perf_open_param *param = reinterpret_cast<drm_i915_perf_open_param *>(arg);
                uint64_t *values = reinterpret_cast<uint64_t *>(param->properties_ptr);
                EXPECT_EQ(values[9], 3ull);
            }
            return 0;
        });
        EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
    }
}

HWTEST2_F(MetricIpSamplingLinuxMultiDeviceTest, GivenEngineInfoIsNullWhenStartMeasurementIsCalledForRootDeviceThenErrorIsReturned, IsPVC) {
    debugManager.flags.ZE_AFFINITY_MASK.set("0.1");

    auto deviceFactory = createDevices(4);
    auto driverHandle = std::make_unique<DriverHandleImp>();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto rootDevice = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), deviceFactory->rootDevices[0], false, &returnValue));
    auto metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*rootDevice));
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    auto drm = static_cast<DrmPrelimMock *>(rootDevice->getOsInterface()->getDriverModel()->as<NEO::Drm>());
    drm->queryEngineInfo1SubDevice();
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingLinuxMultiDeviceTest, GivenEngineInstanceIsNullWhenStartMeasurementIsCalledForRootDeviceThenErrorIsReturned, IsPVC) {
    debugManager.flags.ZE_AFFINITY_MASK.set("0.1");

    auto deviceFactory = createDevices(4);
    auto driverHandle = std::make_unique<DriverHandleImp>();
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto rootDevice = std::unique_ptr<L0::Device>(L0::Device::create(driverHandle.get(), deviceFactory->rootDevices[0], false, &returnValue));
    auto metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*rootDevice));
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_ERROR_UNKNOWN);
}

} // namespace ult
} // namespace L0
