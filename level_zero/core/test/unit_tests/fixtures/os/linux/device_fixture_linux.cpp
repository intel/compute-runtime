/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_fabric.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

namespace L0 {
namespace ult {

void setupFabricDriverModels(Mock<L0::DriverHandle> &driverHandle) {
    setupFabricDriverModels(static_cast<L0::DriverHandle &>(driverHandle));
}

void setupFabricDriverModels(L0::DriverHandle &driverHandle) {
    for (auto &l0device : driverHandle.devices) {
        auto &rootDeviceEnvironment = *l0device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0device->getRootDeviceIndex()];
        if (!rootDeviceEnvironment.osInterface) {
            auto osIface = new NEO::OSInterface();
            auto drmMock = new DrmMockResources(rootDeviceEnvironment);
            drmMock->drmFabric = std::make_unique<NEO::DrmFabricStub>();
            rootDeviceEnvironment.osInterface.reset(osIface);
            rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<NEO::Drm>(drmMock));
        }
    }
}

} // namespace ult
} // namespace L0
