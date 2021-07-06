/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"

namespace NEO {
struct DispatchFlagsTests : public ::testing::Test {
    template <typename CsrType>
    void SetUpImpl() {
        environmentWrapper.setCsrType<CsrType>();
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        context = std::make_unique<MockContext>(device.get());
    }

    EnvironmentWithCsrWrapper environmentWrapper;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    DebugManagerStateRestore restore;
};
} // namespace NEO
