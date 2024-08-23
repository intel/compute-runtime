/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_execution_environment.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include "gtest/gtest.h"

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
bool MockRootDeviceEnvironment::initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex) {
    initOsInterfaceCalled++;
    if (!initOsInterfaceResults.empty()) {
        auto retVal = initOsInterfaceResults[initOsInterfaceCalled - 1];
        if (retVal) {
            RootDeviceEnvironment::initOsInterface(std::move(hwDeviceId), rootDeviceIndex);
        }
        return retVal;
    }
    return RootDeviceEnvironment::initOsInterface(std::move(hwDeviceId), rootDeviceIndex);
}
bool MockRootDeviceEnvironment::initAilConfiguration() {
    if (ailInitializationResult.has_value()) {
        return *ailInitializationResult;
    } else
        return RootDeviceEnvironment::initAilConfiguration();
}

MockRootDeviceEnvironment::~MockRootDeviceEnvironment() {
    if (initOsInterfaceExpectedCallCount) {
        EXPECT_EQ(*initOsInterfaceExpectedCallCount, initOsInterfaceCalled);
    }
}

void MockRootDeviceEnvironment::resetBuiltins(RootDeviceEnvironment *rootDeviceEnvironment, BuiltIns *newValue) {
    if (rootDeviceEnvironment->builtins) {
        rootDeviceEnvironment->builtins->freeSipKernels(rootDeviceEnvironment->executionEnvironment.memoryManager.get());
    }
    rootDeviceEnvironment->builtins.reset(newValue);
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

        UnitTestSetter::setRcsExposure(*rootDeviceEnvironments[rootDeviceIndex]);
        UnitTestSetter::setCcsExposure(*rootDeviceEnvironments[rootDeviceIndex]);
    }

    calculateMaxOsContextCount();
}
void MockExecutionEnvironment::initGmm() {
    for (auto &rootDeviceEnvironment : rootDeviceEnvironments) {
        rootDeviceEnvironment->initGmm();
    }
}

} // namespace NEO
