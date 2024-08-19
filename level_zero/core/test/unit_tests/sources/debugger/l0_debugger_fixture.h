/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
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
    void setUp() {
        setUp(true);
    }
    void setUp(bool createDriver) {

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        auto mockBuiltIns = new NEO::MockBuiltins();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
        memoryOperationsHandler = new NEO::MockMemoryOperations();
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);
        executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);

        hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hwInfo.featureTable.flags.ftrLocalMemory = true;

        auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
        auto isHexadecimalArrayPreferred = gfxCoreHelper.isSipKernelAsHexadecimalArrayPreferred();
        if (isHexadecimalArrayPreferred) {
            MockSipData::useMockSip = true;
        }

        executionEnvironment->calculateMaxOsContextCount();
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        executionEnvironment->initializeMemoryManager();

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

        if (createDriver) {
            NEO::DeviceVector devices;
            devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
            driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
            driverHandle->enableProgramDebugging = NEO::DebuggingMode::online;

            driverHandle->initialize(std::move(devices));
            device = driverHandle->devices[0];
        }
    }

    void tearDown() {
    }

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    NEO::HardwareInfo hwInfo = *defaultHwInfo;
    MockMemoryOperations *memoryOperationsHandler = nullptr;
    VariableBackup<bool> mockSipCalled{&NEO::MockSipData::called};
    VariableBackup<NEO::SipKernelType> mockSipCalledType{&NEO::MockSipData::calledType};
    VariableBackup<bool> backupSipInitType{&MockSipData::useMockSip};
};

struct L0DebuggerHwFixture : public L0DebuggerFixture {
    void setUp() {
        L0DebuggerFixture::setUp();
        debuggerHw = static_cast<DebuggerL0 *>(neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.get());
        neoDevice->setPreemptionMode(PreemptionMode::Disabled);
    }

    void tearDown() {
        L0DebuggerFixture::tearDown();
        debuggerHw = nullptr;
    }
    template <typename GfxFamily>
    MockDebuggerL0Hw<GfxFamily> *getMockDebuggerL0Hw() {
        return static_cast<MockDebuggerL0Hw<GfxFamily> *>(debuggerHw);
    }

    NEO::GraphicsAllocation *allocateIsaMemory(size_t size, bool internal) {
        auto allocType = (internal ? NEO::AllocationType::kernelIsaInternal : NEO::AllocationType::kernelIsa);
        return neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({neoDevice->getRootDeviceIndex(), size, allocType, neoDevice->getDeviceBitfield()});
    }

    DebuggerL0 *debuggerHw = nullptr;
};

struct L0DebuggerPerContextAddressSpaceFixture : public L0DebuggerHwFixture {
    void setUp() {
        NEO::debugManager.flags.DebuggerForceSbaTrackingMode.set(0);
        L0DebuggerHwFixture::setUp();
    }
    void tearDown() {
        L0DebuggerHwFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
};

struct L0DebuggerPerContextAddressSpaceGlobalBindlessFixture : public L0DebuggerHwFixture {
    void setUp() {
        NEO::debugManager.flags.DebuggerForceSbaTrackingMode.set(0);
        NEO::debugManager.flags.UseBindlessMode.set(1);
        NEO::debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
        L0DebuggerHwFixture::setUp();
    }
    void tearDown() {
        L0DebuggerHwFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
};

struct L0DebuggerSingleAddressSpaceFixture : public L0DebuggerHwFixture {
    void setUp() {
        NEO::debugManager.flags.DebuggerForceSbaTrackingMode.set(1);
        L0DebuggerHwFixture::setUp();
    }
    void tearDown() {
        L0DebuggerHwFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
};

struct L0DebuggerHwParameterizedFixture : ::testing::TestWithParam<int>, public L0DebuggerHwFixture {
    void SetUp() override {
        NEO::debugManager.flags.DebuggerForceSbaTrackingMode.set(GetParam());
        L0DebuggerHwFixture::setUp();
    }
    void TearDown() override {
        L0DebuggerHwFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
};

struct L0DebuggerHwGlobalStatelessFixture : public L0DebuggerHwFixture {
    void setUp() {
        NEO::debugManager.flags.SelectCmdListHeapAddressModel.set(1);
        L0DebuggerHwFixture::setUp();
    }

    DebugManagerStateRestore restorer;
};

} // namespace ult
} // namespace L0
