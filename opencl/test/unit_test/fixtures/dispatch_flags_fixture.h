/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {
template <bool setupBlitter>
struct DispatchFlagsTestsBase : public ::testing::Test {
    template <typename CsrType>
    void SetUpImpl() {
        HardwareInfo hwInfo = *defaultHwInfo;
        if (setupBlitter) {
            hwInfo.capabilityTable.blitterOperationsSupported = true;
        }

        environmentWrapper.setCsrType<CsrType>();
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
        context = std::make_unique<MockContext>(device.get());
    }

    EnvironmentWithCsrWrapper environmentWrapper;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    DebugManagerStateRestore restore;
};

using DispatchFlagsTests = DispatchFlagsTestsBase<false>;
using DispatchFlagsBlitTests = DispatchFlagsTestsBase<true>;
} // namespace NEO
