/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/mock_source_level_debugger.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

namespace L0 {
namespace ult {

struct ActiveDebuggerFixture {
    void setUp() {

        ze_result_t returnValue;
        auto executionEnvironment = new NEO::MockExecutionEnvironment();
        auto mockBuiltIns = new MockBuiltins();

        hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

        executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        auto isHexadecimalArrayPrefered = HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSipKernelAsHexadecimalArrayPreferred();
        if (isHexadecimalArrayPrefered) {
            MockSipData::useMockSip = true;
        }

        debugger = new MockActiveSourceLevelDebugger(new MockOsLibrary);
        executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(debugger);
        executionEnvironment->initializeMemoryManager();
        executionEnvironment->setDebuggingEnabled();

        device = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);
        device->setDebuggerActive(true);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(device));

        auto driverHandleUlt = whiteboxCast(DriverHandle::create(std::move(devices), L0EnvVariables{}, &returnValue));
        driverHandle.reset(driverHandleUlt);

        ASSERT_NE(nullptr, driverHandle);

        ze_device_handle_t hDevice;
        uint32_t count = 1;
        ze_result_t result = driverHandle->getDevice(&count, &hDevice);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        deviceL0 = L0::Device::fromHandle(hDevice);
        ASSERT_NE(nullptr, deviceL0);
    }
    void tearDown() {
        L0::GlobalDriver = nullptr;
    }

    std::unique_ptr<L0::ult::WhiteBox<L0::DriverHandle>> driverHandle;
    NEO::MockDevice *device = nullptr;
    L0::Device *deviceL0;
    MockActiveSourceLevelDebugger *debugger = nullptr;
    HardwareInfo hwInfo;
    VariableBackup<bool> mockSipCalled{&NEO::MockSipData::called};
    VariableBackup<NEO::SipKernelType> mockSipCalledType{&NEO::MockSipData::calledType};
    VariableBackup<bool> backupSipInitType{&MockSipData::useMockSip};
};
} // namespace ult
} // namespace L0
