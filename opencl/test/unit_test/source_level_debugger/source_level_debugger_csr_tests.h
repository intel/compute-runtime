/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/mock_source_level_debugger.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include <memory>

class CommandStreamReceiverWithActiveDebuggerTest : public ::testing::Test {
  protected:
    template <typename FamilyType>
    auto createCSR() {
        hwInfo = nullptr;
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<MockCsrHw2<FamilyType>>();
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
        hwInfo->capabilityTable = defaultHwInfo->capabilityTable;
        hwInfo->capabilityTable.debuggerSupported = true;

        auto mockMemoryManager = new MockMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(mockMemoryManager);

        executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockActiveSourceLevelDebugger(new MockOsLibrary));

        device = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0));
        device->setSourceLevelDebuggerActive(true);

        return static_cast<MockCsrHw2<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    }

    void TearDown() override {
        device->setSourceLevelDebuggerActive(false);
    }

    std::unique_ptr<MockClDevice> device;
    ExecutionEnvironment *executionEnvironment = nullptr;
    HardwareInfo *hwInfo = nullptr;
};
