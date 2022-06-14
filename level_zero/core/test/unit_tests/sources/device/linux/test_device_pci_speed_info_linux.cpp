/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/mocks/mock_driver_model.h"

#include "level_zero/core/test/unit_tests/sources/device/test_device_pci_speed_info.h"

namespace L0 {
namespace ult {

void PciSpeedInfoTest::setPciSpeedInfo(NEO::ExecutionEnvironment *executionEnvironment, const NEO::PhyicalDevicePciSpeedInfo &pciSpeedInfo) {
    auto driverModel = std::make_unique<NEO::MockDriverModel>();
    driverModel->pciSpeedInfo = pciSpeedInfo;
    driverModel->pciBusInfo = {0, 1, 2, 3};
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::move(driverModel));
}

} // namespace ult
} // namespace L0
