/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/driver_info.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"

#include "level_zero/core/test/unit_tests/sources/device/test_device_pci_bus_info.h"

namespace L0 {
namespace ult {

class PciBusInfoTestDriverModel : public WddmMock {
  public:
    PciBusInfoTestDriverModel(RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment) {}
    void setExpectedPciBusInfo(const PhysicalDevicePciBusInfo &pciBusInfo) {
        returnedBusInfo = pciBusInfo;
    }

    PhysicalDevicePciSpeedInfo getPciSpeedInfo() const override {
        return NEO::PhysicalDevicePciSpeedInfo{1, 1, 1};
    }
    PhysicalDevicePciBusInfo getPciBusInfo() const override {
        return returnedBusInfo;
    }

    PhysicalDevicePciBusInfo returnedBusInfo{0, 0, 0, 0};
};

void PciBusInfoTest::setPciBusInfo(NEO::ExecutionEnvironment *executionEnvironment, const NEO::PhysicalDevicePciBusInfo &pciBusInfo, const uint32_t rootDeviceIndex) {
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(
        std::make_unique<PciBusInfoTestDriverModel>(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]));
    PciBusInfoTestDriverModel *driverModel = static_cast<PciBusInfoTestDriverModel *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel());
    driverModel->setExpectedPciBusInfo(pciBusInfo);
}

} // namespace ult
} // namespace L0
