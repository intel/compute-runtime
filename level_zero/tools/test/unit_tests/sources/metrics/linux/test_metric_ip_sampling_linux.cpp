/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// _MAC delete #include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_fixture.h"

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

HWTEST2_F(MetricIpSamplingLinuxTest, GivenLinuxSupportIsAvailableThenIpSamplingSourceIsAvailable, HasIPSamplingSupport) {
    auto &metricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    metricSource.setMetricOsInterface(metricIpSamplingOsInterface);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getMetricDeviceContext().enableMetricApi());
    EXPECT_TRUE(metricSource.isAvailable());
}

} // namespace ult
} // namespace L0
