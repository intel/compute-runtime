/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/os_interface/os_context.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_execution_environment.h"

namespace NEO {
struct DispatchFlagsTests : public ::testing::Test {
    template <typename CsrType>
    void SetUpImpl() {
        auto executionEnvironment = new MockExecutionEnvironmentWithCsr<CsrType>(**platformDevices, 1u);
        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(*platformDevices, executionEnvironment, 0));
        context = std::make_unique<MockContext>(device.get());
    }

    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    DebugManagerStateRestore restore;
};
} // namespace NEO
