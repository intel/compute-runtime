/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_sip.h"

#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

namespace L0 {
namespace ult {

struct L0DebuggerFixture {
    void SetUp() { // NOLINT(readability-identifier-naming)
        NEO::MockCompilerEnableGuard mock(true);
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        auto mockBuiltIns = new NEO::MockBuiltins();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        memoryOperationsHandler = new NEO::MockMemoryOperations();
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);
        executionEnvironment->setDebuggingEnabled();

        hwInfo = *NEO::defaultHwInfo.get();
        hwInfo.featureTable.flags.ftrLocalMemory = true;

        auto isHexadecimalArrayPrefered = HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSipKernelAsHexadecimalArrayPreferred();
        if (isHexadecimalArrayPrefered) {
            MockSipData::useMockSip = true;
        }

        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        executionEnvironment->initializeMemoryManager();

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->enableProgramDebugging = true;

        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void TearDown() { // NOLINT(readability-identifier-naming)
    }

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    NEO::HardwareInfo hwInfo;
    MockMemoryOperations *memoryOperationsHandler = nullptr;
    VariableBackup<bool> mockSipCalled{&NEO::MockSipData::called};
    VariableBackup<NEO::SipKernelType> mockSipCalledType{&NEO::MockSipData::calledType};
    VariableBackup<bool> backupSipInitType{&MockSipData::useMockSip};
};

struct L0DebuggerHwFixture : public L0DebuggerFixture {
    void SetUp() {
        L0DebuggerFixture::SetUp();
        debuggerHw = static_cast<DebuggerL0 *>(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.get());
        neoDevice->setPreemptionMode(PreemptionMode::Disabled);
    }

    void TearDown() {
        L0DebuggerFixture::TearDown();
        debuggerHw = nullptr;
    }
    template <typename GfxFamily>
    MockDebuggerL0Hw<GfxFamily> *getMockDebuggerL0Hw() {
        return static_cast<MockDebuggerL0Hw<GfxFamily> *>(debuggerHw);
    }
    DebuggerL0 *debuggerHw = nullptr;
};

struct L0DebuggerPerContextAddressSpaceFixture : public L0DebuggerHwFixture {
    void SetUp() {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(0);
        L0DebuggerHwFixture::SetUp();
    }
    void TearDown() {
        L0DebuggerHwFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

struct L0DebuggerHwParameterizedFixture : ::testing::TestWithParam<int>, public L0DebuggerHwFixture {
    void SetUp() override {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(GetParam());
        L0DebuggerHwFixture::SetUp();
    }
    void TearDown() override {
        L0DebuggerHwFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

} // namespace ult
} // namespace L0
