/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/mocks/mock_driver_model.h"

#include "level_zero/core/test/unit_tests/sources/device/test_device_pci_bus_info.h"

namespace L0 {
namespace ult {

void PciBusInfoTest::setPciBusInfo(NEO::ExecutionEnvironment *executionEnvironment, const NEO::PhysicalDevicePciBusInfo &pciBusInfo, const uint32_t rootDeviceIndex) {
    auto driverModel = std::make_unique<NEO::MockDriverModel>();
    driverModel->pciSpeedInfo = {1, 1, 1};
    driverModel->pciBusInfo = pciBusInfo;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::move(driverModel));
}

} // namespace ult
} // namespace L0
