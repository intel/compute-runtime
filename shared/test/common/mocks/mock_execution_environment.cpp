/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_execution_environment.h"

#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"

namespace NEO {
extern bool useMockGmm;

void MockRootDeviceEnvironment::initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (!initAubCenterCalled) {
        initAubCenterCalled = true;
        localMemoryEnabledReceived = localMemoryEnabled;
        aubFileNameReceived = aubFileName;
    }
    if (useMockAubCenter) {
        MockAubCenterFixture::setMockAubCenter(*this);
    }
    RootDeviceEnvironment::initAubCenter(localMemoryEnabled, aubFileName, csrType);
}

MockExecutionEnvironment::MockExecutionEnvironment() : MockExecutionEnvironment(defaultHwInfo.get()) {}
MockExecutionEnvironment::MockExecutionEnvironment(const HardwareInfo *hwInfo) : MockExecutionEnvironment(hwInfo, true, 1u) {
}
MockExecutionEnvironment::MockExecutionEnvironment(const HardwareInfo *hwInfo, bool useMockAubCenter, uint32_t numRootDevices) {
    prepareRootDeviceEnvironments(numRootDevices);
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        auto rootDeviceEnvironment = new MockRootDeviceEnvironment(*this);
        rootDeviceEnvironment->useMockAubCenter = useMockAubCenter;
        rootDeviceEnvironments[rootDeviceIndex].reset(rootDeviceEnvironment);

        if (hwInfo) {
            rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(hwInfo);
        } else {
            rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        }
        if (useMockGmm) {
            rootDeviceEnvironments[rootDeviceIndex]->initGmm();
        }
    }

    calculateMaxOsContextCount();
}
void MockExecutionEnvironment::initGmm() {
    for (auto &rootDeviceEnvironment : rootDeviceEnvironments) {
        rootDeviceEnvironment->initGmm();
    }
}

} // namespace NEO
