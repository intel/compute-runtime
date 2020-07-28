/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "test.h"

#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

class MetricQueryPoolLinuxTest : public MetricContextFixture,
                                 public ::testing::Test {
  public:
    void SetUp() override {
        MetricContextFixture::SetUp();
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto osInterface = device->getOsInterface().get();
        osInterface->setDrm(new DrmMock(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
    }

    void TearDown() override {
        MetricContextFixture::TearDown();
    }
};

TEST_F(MetricQueryPoolLinuxTest, givenCorrectArgumentsWhenGetContextDataIsCalledThenReturnsSuccess) {

    ClientData_1_0 clientData = {};
    ContextCreateData_1_0 contextData = {};
    ClientDataLinuxAdapter_1_0 adapter = {};

    adapter.Type = LinuxAdapterType::Last;
    adapter.DrmFileDescriptor = -1;

    clientData.Linux.Adapter = &adapter;
    contextData.ClientData = &clientData;
    contextData.ClientData->Linux.Adapter = &adapter;

    EXPECT_EQ(mockMetricsLibrary->metricsLibraryGetContextData(*device, contextData), true);

    auto osInterface = device->getOsInterface().get();

    EXPECT_EQ(contextData.ClientData->Linux.Adapter->DrmFileDescriptor, osInterface->getDrm()->getFileDescriptor());
    EXPECT_EQ(contextData.ClientData->Linux.Adapter->Type, LinuxAdapterType::DrmFileDescriptor);
}

} // namespace ult
} // namespace L0
