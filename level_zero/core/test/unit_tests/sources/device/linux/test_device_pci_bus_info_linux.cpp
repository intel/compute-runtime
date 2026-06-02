/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/drm_fabric.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "level_zero/core/test/unit_tests/sources/device/test_device_pci_bus_info.h"

namespace L0 {
namespace ult {

void PciBusInfoTest::setPciBusInfo(NEO::ExecutionEnvironment *executionEnvironment, const NEO::PhysicalDevicePciBusInfo &pciBusInfo, const uint32_t rootDeviceIndex) {
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex];
    auto drmMock = new DrmMockResources(rootDeviceEnvironment);
    drmMock->drmFabric = std::make_unique<NEO::DrmFabricStub>();
    drmMock->adapterBDF.bus = static_cast<uint32_t>(pciBusInfo.pciBus);
    drmMock->adapterBDF.device = static_cast<uint32_t>(pciBusInfo.pciDevice);
    drmMock->adapterBDF.function = static_cast<uint32_t>(pciBusInfo.pciFunction);
    drmMock->adapterBDF.reserved = 0;
    drmMock->setPciDomain(pciBusInfo.pciDomain);
    rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<NEO::Drm>(drmMock));
}

} // namespace ult
} // namespace L0
