/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/windows/wddm_fixture.h"

#include "shared/test/common/mocks/mock_device.h"

namespace NEO {

void WddmInstrumentationGmmFixture::setUp() {
    DeviceFixture::setUp();
    executionEnvironment = pDevice->getExecutionEnvironment();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    gmmMem = new MockGmmMemory(rootDeviceEnvironment->getGmmClientContext());
    wddm->gmmMemory.reset(gmmMem);
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
}

} // namespace NEO
