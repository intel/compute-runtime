/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "per_product_test_definitions.h"

#include <string>

using namespace NEO;

using CriProgramInternalOptionsTests = ::Test<ClDeviceFixture>;

CRITEST_F(CriProgramInternalOptionsTests, WhenProgramGetInternalOptionsThenInternalOptionsAreCorrect) {
    DebugManagerStateRestore restore{};

    auto flagEnabled = "-ze-intel-64bit-addressing";

    {
        MockProgram program(toClDeviceVector(*pClDevice));
        debugManager.flags.Enable64BitAddressing.set(-1);
        auto internalOptions = program.getInternalOptions();
        auto position = internalOptions.find(flagEnabled);
        EXPECT_NE(std::string::npos, position);
    }
    {
        MockProgram program(toClDeviceVector(*pClDevice));
        debugManager.flags.Enable64BitAddressing.set(0);
        auto internalOptions = program.getInternalOptions();
        auto position = internalOptions.find(flagEnabled);
        EXPECT_EQ(std::string::npos, position);
    }
    {
        MockProgram program(toClDeviceVector(*pClDevice));
        debugManager.flags.Enable64BitAddressing.set(1);
        auto internalOptions = program.getInternalOptions();
        auto position = internalOptions.find(flagEnabled);
        EXPECT_NE(std::string::npos, position);
    }
}
