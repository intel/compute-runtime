/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

namespace NEO {

extern GMM_INIT_IN_ARGS passedInputArgs;
extern bool copyInputArgs;

TEST(GmmHelperTest, whenCreateGmmHelperWithoutOsInterfaceThenPassedAdapterBDFIsZeroed) {
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    ADAPTER_BDF expectedAdapterBDF{};

    MockExecutionEnvironment executionEnvironment{};
    EXPECT_EQ(0, memcmp(&expectedAdapterBDF, &passedInputArgs.stAdapterBDF, sizeof(ADAPTER_BDF)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);
}

} // namespace NEO
