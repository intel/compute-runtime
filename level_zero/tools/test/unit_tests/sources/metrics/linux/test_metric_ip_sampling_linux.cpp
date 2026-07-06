/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/linux/os_metric_ip_sampling_imp_linux.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling.h"

namespace L0 {
extern std::vector<_ze_driver_handle_t *> *globalDriverHandles;
namespace ult {

class MetricIpSamplingLinuxTest : public DeviceFixture,
                                  public ::testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
        auto &rootDeviceEnvironment = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()];
        auto mockDrm = std::make_unique<DrmMockExtended>(rootDeviceEnvironment);
        auto mockIoctlHelper = std::make_unique<MockIoctlHelper>(*mockDrm);
        mockDrm->ioctlHelper.reset(mockIoctlHelper.release());
        rootDeviceEnvironment.osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = *device->getOsInterface();
        osInterface.setDriverModel(std::move(mockDrm));
        metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*device));

        globalDriverHandles->push_back(driverHandle.get());
        auto mockProductHelperHw = std::make_unique<MockProductHelperHw<IGFX_UNKNOWN>>();
        std::unique_ptr<ProductHelper> mockProductHelper = std::move(mockProductHelperHw);
        std::swap(rootDeviceEnvironment.productHelper, mockProductHelper);
    }

    void TearDown() override {
        DeviceFixture::tearDown();
        globalDriverHandles->clear();
    }
    std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface = nullptr;
};

HWTEST2_F(MetricIpSamplingLinuxTest, GivenEuStallSupportedWhenIsOsSupportAvailableIsCalledThenReturnTrue, HasIPSamplingSupport) {
    auto drm = device->getOsInterface()->getDriverModel()->as<NEO::Drm>();
    auto mockIoctlHelper = static_cast<MockIoctlHelper *>(drm->getIoctlHelper());
    mockIoctlHelper->isEuStallSupportedResult = true;
    EXPECT_TRUE(metricIpSamplingOsInterface->isOsSupportAvailable());
}

HWTEST2_F(MetricIpSamplingLinuxTest, GivenEuStallNotSupportedWhenIsOsSupportAvailableIsCalledThenReturnFalse, HasIPSamplingSupport) {
    auto drm = device->getOsInterface()->getDriverModel()->as<NEO::Drm>();
    auto mockIoctlHelper = static_cast<MockIoctlHelper *>(drm->getIoctlHelper());
    mockIoctlHelper->isEuStallSupportedResult = false;
    EXPECT_FALSE(metricIpSamplingOsInterface->isOsSupportAvailable());
}

HWTEST2_F(MetricIpSamplingLinuxTest, GivenEuStallMaxReportsWhenClampNReportsIsCalledThenValueIsClampedToEnabledXeCoreLimit, HasIPSamplingSupport) {
    auto drm = device->getOsInterface()->getDriverModel()->as<NEO::Drm>();
    auto mockIoctlHelper = static_cast<MockIoctlHelper *>(drm->getIoctlHelper());
    auto linuxIface = static_cast<MetricIpSamplingLinuxImp *>(metricIpSamplingOsInterface.get());

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.gtSystemInfo.DualSubSliceCount = 2u;

    // > 0: real per-Xe-core capacity scaled by enabled Xe-cores (8192 * 2 = 16384).
    mockIoctlHelper->getEuStallMaxReportsPerXeCoreResult = 8192;
    uint32_t notifyEveryNReports = 32768u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxIface->clampNReports(notifyEveryNReports));
    EXPECT_EQ(16384u, notifyEveryNReports);
    notifyEveryNReports = 16384u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxIface->clampNReports(notifyEveryNReports));
    EXPECT_EQ(16384u, notifyEveryNReports);
    notifyEveryNReports = 3000u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxIface->clampNReports(notifyEveryNReports));
    EXPECT_EQ(3000u, notifyEveryNReports);

    // == 0: no KMD query (i915/PVC or older xe) -> static estimate
    // (maxDssBufferSize/unitReportSize) * MaxDualSubSlicesSupported = 8192 * 2 = 16384.
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = 2u;
    mockIoctlHelper->getEuStallMaxReportsPerXeCoreResult = 0;
    notifyEveryNReports = 32768u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxIface->clampNReports(notifyEveryNReports));
    EXPECT_EQ(16384u, notifyEveryNReports);
    notifyEveryNReports = 3000u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxIface->clampNReports(notifyEveryNReports));
    EXPECT_EQ(3000u, notifyEveryNReports);

    // < 0: query present but failed -> abort.
    mockIoctlHelper->getEuStallMaxReportsPerXeCoreResult = -1;
    notifyEveryNReports = 32768u;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, linuxIface->clampNReports(notifyEveryNReports));

    // A zero enabled Xe-core count on the queried path is a topology-query invariant violation.
    mockIoctlHelper->getEuStallMaxReportsPerXeCoreResult = 8192;
    hwInfo.gtSystemInfo.DualSubSliceCount = 0u;
    notifyEveryNReports = 32768u;
    EXPECT_THROW(linuxIface->clampNReports(notifyEveryNReports), std::exception);
}

HWTEST2_F(MetricIpSamplingLinuxTest, GivenLinuxSupportIsAvailableThenIpSamplingSourceIsAvailable, HasIPSamplingSupport) {
    auto &metricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    auto mockOsInterface = new MockMetricIpSamplingOsInterface();
    std::unique_ptr<MetricIpSamplingOsInterface> mockOsInterfacePtr(mockOsInterface);
    metricSource.setMetricOsInterface(mockOsInterfacePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getMetricDeviceContext().enableMetricApi());
    EXPECT_TRUE(metricSource.isAvailable());
}

} // namespace ult
} // namespace L0
