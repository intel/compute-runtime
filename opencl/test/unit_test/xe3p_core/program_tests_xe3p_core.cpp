/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

#include <string>

using namespace NEO;

using Xe3pProgramInternalOptionsTests = ::Test<ClDeviceFixture>;

XE3P_CORETEST_F(Xe3pProgramInternalOptionsTests, WhenProgramGetInternalOptionsThenInternalOptionsAreCorrect) {

    auto flagEnabled = "-ze-intel-64bit-addressing";
    MockProgram program(toClDeviceVector(*pClDevice));

    auto internalOptions = program.getInternalOptions();
    auto position = internalOptions.find(flagEnabled);
    EXPECT_NE(std::string::npos, position);
}
