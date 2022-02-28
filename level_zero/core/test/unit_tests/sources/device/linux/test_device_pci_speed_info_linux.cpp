/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/sources/device/test_device_pci_speed_info.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

class PciSpeedInfoTestDriverModel : public MockDriverModel {
  public:
    PciSpeedInfoTestDriverModel() : MockDriverModel(0) {}

    void setExpectedPciSpeedInfo(const PhyicalDevicePciSpeedInfo &pciSpeedInfo) {
        returnedSpeedInfo = pciSpeedInfo;
    }

    NEO::PhyicalDevicePciSpeedInfo getPciSpeedInfo() const override {
        return returnedSpeedInfo;
    }
    NEO::PhysicalDevicePciBusInfo getPciBusInfo() const override {
        return NEO::PhysicalDevicePciBusInfo(0, 1, 2, 3);
    }
    PhyicalDevicePciSpeedInfo returnedSpeedInfo = {-1, -1, -1};
};

void PciSpeedInfoTest::setPciSpeedInfo(NEO::ExecutionEnvironment *executionEnvironment, const NEO::PhyicalDevicePciSpeedInfo &pciSpeedInfo) {
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(
        std::make_unique<PciSpeedInfoTestDriverModel>());
    PciSpeedInfoTestDriverModel *driverModel = static_cast<PciSpeedInfoTestDriverModel *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel());
    driverModel->setExpectedPciSpeedInfo(pciSpeedInfo);
}

} // namespace ult
} // namespace L0
